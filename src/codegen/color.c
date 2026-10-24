#include "codegen/color.h"

#include <string.h>

#define LOOP_WEIGHT 8u
#define MIN_HOME_SCORE 3u

typedef struct ColorRegs {
	Arena *arena;
	IR *ir;
	IRFn *fn;
	Reg *regs;
	RegCtx *ctx;
	Cfg *cfg;
	uint32_t num_regs;
} ColorRegs;

static int tail_self (ColorRegs *cr, uint32_t i)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	IRInstr *in = ir->instrs + fn->start + i;
	IRInstr *end = ir->instrs + fn->start + fn->count;
	IRInstr *n = in + 1;
	uint8_t from = ir->fns[in->target].ret_type;
	uint8_t to = fn->ret_type;

	while (n < end && n->op == IR_LABEL) n++;
	if (n >= end || n->op != IR_RET) return 0;
	if (n->src1 != in->dst || in->argc > 6) return 0;
	if (in->dst != NO_REG && cr->regs[in->dst].uses != 1) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

static void loop_weights (ColorRegs *cr, int16_t *w)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	uint32_t labels = ir->label_count + 1;
	uint32_t *lpos = arena_alloc(cr->arena, labels * sizeof(uint32_t));
	uint32_t self = (uint32_t)(fn - ir->fns);

	for (uint32_t i = 0; i < labels; i++)
		lpos[i] = UINT32_MAX;
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		if (in->op == IR_LABEL && in->target < labels)
			lpos[in->target] = i;
	}

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t t = UINT32_MAX;

		if (in->op == IR_JMP || in->op == IR_JZ || in->op == IR_JNZ)
			t = in->target < labels ? lpos[in->target] : UINT32_MAX;
		else if (in->op == IR_CALL && in->target == self && tail_self(cr, i))
			t = 0;

		if (t == UINT32_MAX || t > i) continue;
		w[t]++;
		if (i + 1 < fn->count) w[i + 1]--;
	}

	int16_t d = 0;
	for (uint32_t i = 0; i < fn->count; i++) {
		d = (int16_t)(d + w[i]);
		w[i] = d > 0 ? LOOP_WEIGHT : 1;
	}
}

static void weigh_regs (ColorRegs *cr, const int16_t *w)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		for (int k = 0; k < n; k++)
			cr->regs[*src[k]].wscore += (uint32_t)w[i];
		if (in->dst != NO_REG)
			cr->regs[in->dst].wscore += (uint32_t)w[i];
	}
}

static void mark_weighted (ColorRegs *cr)
{
	uint32_t count = cr->fn->count ? cr->fn->count : 1;
	int16_t *w = arena_alloc(cr->arena, count * sizeof(int16_t));

	memset(w, 0, count * sizeof(int16_t));
	loop_weights(cr, w);
	weigh_regs(cr, w);
}

static void cross_call (ColorRegs *cr, uint8_t *cross, uint32_t v)
{
	uint32_t root = cr->regs[v].cstate == VR_ALIAS ? cr->regs[v].slot : v;

	cross[v] = 1;
	cross[root] = 1;
}

static void cross_instr (ColorRegs *cr, const uint64_t *tmp, uint8_t *cross)
{
	uint32_t words = (cr->fn->reg_count + 63) >> 6;

	for (uint32_t w = 0; w < words; w++) {
		uint64_t bits = tmp[w];

		while (bits) {
			cross_call(cr, cross, (w << 6) + (uint32_t)__builtin_ctzll(bits));
			bits &= bits - 1;
		}
	}
}

static void cross_block (ColorRegs *cr, uint64_t *tmp, uint32_t b, uint8_t *cross)
{
	CfgBlock *blk = cr->cfg->blocks + b;
	IRInstr *code = cr->ir->instrs + blk->start;

	cfg_live_out(cr->cfg, b, tmp, (cr->fn->reg_count + 63) >> 6);
	for (uint32_t i = blk->count; i-- > 0;) {
		IRInstr *in = code + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		if (in->dst != NO_REG)
			tmp[in->dst >> 6] &= ~(1ull << (in->dst & 63));
		if (in->op == IR_CALL) cross_instr(cr, tmp, cross);
		for (int k = 0; k < n; k++)
			tmp[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	}
}

static void mark_crossing (ColorRegs *cr, uint64_t *tmp, uint8_t *cross)
{
	for (uint32_t b = 0; b < cr->cfg->block_count; b++)
		cross_block(cr, tmp, b, cross);
}

static uint32_t list_mask (uint64_t mask, uint8_t *regs, uint32_t cap)
{
	uint32_t n = 0;

	while (mask && n < cap) {
		regs[n++] = (uint8_t)__builtin_ctzll(mask);
		mask &= mask - 1;
	}
	return n;
}

static uint32_t rank_picks (ColorRegs *cr, uint32_t *pick)
{
	IRFn *fn = cr->fn;
	Reg *regs = cr->regs;
	uint32_t n = 0;

	for (uint32_t r = 0; r < fn->reg_count; r++) {
		uint32_t k = 0;

		if (regs[r].cstate != VR_MEM || !regs[r].uses) continue;
		if (regs[r].wscore < MIN_HOME_SCORE) continue;
		while (k < n && regs[pick[k]].wscore >= regs[r].wscore) k++;
		if (k == cr->num_regs) continue;

		uint32_t j = n < cr->num_regs ? n : cr->num_regs - 1;
		while (j > k) {
			pick[j] = pick[j - 1];
			j--;
		}
		pick[k] = r;
		if (n < cr->num_regs) n++;
	}
	return n;
}

static void set_reg_home (Reg *reg, uint32_t block_count, int h)
{
	for (uint32_t b = 0; b < block_count; b++)
		reg->home[b] = (int8_t)h;
}

// Temporary: reserve every fixed register of the ops present
static uint64_t needs_reserved (ColorRegs *cr)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	uint64_t m = 0;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		OpNeeds *n = &cr->ctx->needs[in->op];

		m |= n->destroy_mask;
		m |= n->src1.type == REG_COND_FIXED ? 1ull << n->src1.reg : 0;
		m |= n->src2.type == REG_COND_FIXED ? 1ull << n->src2.reg : 0;
		m |= n->dst.type == REG_COND_FIXED ? 1ull << n->dst.reg : 0;
	}
	return m;
}

static void color_run (ColorRegs *cr)
{
	if (!cr->fn->reg_count) return;

	mark_weighted(cr);

	uint8_t *cross = arena_alloc(cr->arena, cr->fn->reg_count);
	memset(cross, 0, cr->fn->reg_count);
	uint64_t *tmp = arena_alloc(cr->arena, ((cr->fn->reg_count + 63) >> 6) * sizeof(uint64_t));
	mark_crossing(cr, tmp, cross);

	// change this line for real avliable by block
	uint64_t avail = (cr->ctx->caller_mask | cr->ctx->callee_mask) & ~needs_reserved(cr);

	uint32_t caller_n = (uint32_t)__builtin_popcountll(cr->ctx->caller_mask);
	uint32_t callee_n = (uint32_t)__builtin_popcountll(cr->ctx->callee_mask);
	uint8_t callreg[caller_n ? caller_n : 1];
	uint8_t calreg[callee_n ? callee_n : 1];
	uint32_t pick[cr->num_regs];

	uint32_t nc = list_mask(cr->ctx->caller_mask & avail, callreg, caller_n);
	uint32_t nn = list_mask(cr->ctx->callee_mask & avail, calreg, callee_n);

	// change from here to the function end for the graph coloring algorithm
	uint32_t n = rank_picks(cr, pick);
	uint32_t cidx = 0;
	uint32_t lidx = 0;

	for (uint32_t k = 0; k < n; k++) {
		uint32_t r = pick[k];

		if (!cross[r] && cidx < nc)
			set_reg_home(cr->regs + r, cr->ctx->block_count, callreg[cidx++]);
		else if (lidx < nn)
			set_reg_home(cr->regs + r, cr->ctx->block_count, calreg[lidx++]);
	}
}

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg, uint32_t reg_num)
{
	ColorRegs cr = {
		.arena = arena,
		.ir = ir,
		.fn = fn,
		.regs = regs,
		.ctx = ctx,
		.cfg = cfg,
		.num_regs = reg_num,
	};
	color_run(&cr);
}
