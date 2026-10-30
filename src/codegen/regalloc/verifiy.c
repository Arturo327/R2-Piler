#include "codegen/regalloc/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERIFY_REGS 64u

typedef struct Verify {
	RegAlloc *ra;
	uint32_t *alias;
	uint32_t *stamp;
	uint32_t *owner;
	uint32_t epoch;
	uint32_t at;
} Verify;

static void verify_fail (Verify *v, const char *what)
{
	fprintf(stderr, "internal error: regalloc contract broken in '%.*s', instr %u: %s\n",
			(int)v->ra->fn->len, v->ra->fn->name, v->at, what);
	exit(1);
}

static void verify_home (Verify *v, uint32_t r, int h)
{
	RegAlloc *ra = v->ra;
	const ArchDesc *arch = ra->ctx->arch;
	Reg *reg = ra->regs + r;

	if (h < VR_SPILL || h >= (int)arch->reg_num)
		verify_fail(v, "vreg home out of range");
	if (h == VR_SPILL && !reg->slot)
		verify_fail(v, "spilled vreg has no stack slot");
	if (h == VR_SPILL && !reg->spill && reg->cstate != VR_ALIAS)
		verify_fail(v, "spilled vreg is not flagged in Reg.spill");
	if (h >= 0 && (arch->callee_mask >> h & 1) && !(ra->ctx->callee_used >> h & 1))
		verify_fail(v, "callee-saved home missing from callee_used");
}

static void verify_vregs (Verify *v)
{
	RegAlloc *ra = v->ra;

	for (uint32_t r = 0; r < ra->fn->reg_count; r++) {
		const int8_t *home = ra->regs[r].home;

		if (!types[ra->fn->reg_types[r]].size)
			verify_fail(v, "vreg has an unsized type");
		for (uint32_t b = 0; b < ra->ctx->block_count; b++)
			verify_home(v, r, home[b]);
	}
}

static void verify_used_home (Verify *v, uint32_t vreg, uint32_t blk)
{
	Reg *reg = v->ra->regs + vreg;

	if (reg->cstate == VR_CONST || reg->cstate == VR_GLOBAL) return;
	if (reg->home[blk] == VR_NO_HOME)
		verify_fail(v, "used vreg has no home in this block");
}

static void verify_instr_homes (Verify *v, IRInstr *in, uint32_t blk)
{
	Reg *regs = v->ra->regs;
	if (in->op == IR_NOP) return;

	uint32_t *src[3];
	int n = get_srcs(in, src);
	for (int k = 0; k < n; k++)
		verify_used_home(v, *src[k], blk);

	if (in->dst == NO_REG || !regs[in->dst].uses) return;
	if (regs[in->dst].cstate != VR_MEM) return;
	verify_used_home(v, in->dst, blk);
}

static void verify_cstate (Verify *v, IRInstr *in)
{
	if (in->dst == NO_REG) return;
	uint8_t c = v->ra->regs[in->dst].cstate;
	if ((c == VR_CONST || c == VR_ALIAS || c == VR_GLOBAL) && !is_deletable[in->op])
		verify_fail(v, "CONST/ALIAS/GLOBAL cstate on a non-deletable instruction");
}

static void verify_save (Verify *v, IRInstr *in, uint32_t i)
{
	RegAlloc *ra = v->ra;
	uint64_t s = ra->ctx->save[i];
	uint64_t dst_bit = 0;
	int8_t h = VR_NO_HOME;

	if (s & ra->ctx->arch->callee_mask)
		verify_fail(v, "save[] lists a callee-saved home");
	if (in->op != IR_CALL) {
		if (s) verify_fail(v, "save[] set outside a call");
		return;
	}
	if (in->dst != NO_REG) h = ra->regs[in->dst].home[ra->ctx->ins_block[i]];
	if (h >= 0 && ra->regs[in->dst].cstate != VR_CONST) dst_bit = 1ull << h;
	if (s & dst_bit)
		verify_fail(v, "save[] contains the home of the call's dst");
	if (s && cg_is_tail_call(ra->ir, ra->fn, ra->regs, ra->ctx, in,
			ra->ctx->arch->arg_reg_max))
		verify_fail(v, "save[] set on a tail call");
}

static void verify_instrs (Verify *v)
{
	RegAlloc *ra = v->ra;
	IRInstr *code = ra->ir->instrs + ra->fn->start;
	uint64_t bad = ra->ctx->arch->callee_mask & ~ra->ctx->callee_used;

	for (uint32_t i = 0; i < ra->fn->count; i++) {
		uint32_t blk = ra->ctx->ins_block[i];

		v->at = i;
		if (blk >= ra->ctx->block_count)
			verify_fail(v, "ins_block out of range");
		if (ra->ctx->free_regs[i] & bad)
			verify_fail(v, "free_regs lists a callee-saved reg not in callee_used");
		verify_instr_homes(v, code + i, blk);
		verify_cstate(v, code + i);
		verify_save(v, code + i, i);
	}
}

static void verify_setup (Verify *v)
{
	RegAlloc *ra = v->ra;
	IRFn *fn = ra->fn;
	uint32_t max_slot = 0;
	size_t bytes;

	v->alias = arena_alloc(ra->arena, (size_t)fn->reg_count * sizeof(uint32_t));
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		v->alias[r] = r;
		if (ra->regs[r].slot > max_slot) max_slot = ra->regs[r].slot;
	}
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ra->ir->instrs + fn->start + i;

		if (in->op == IR_MOVE && ra->regs[in->dst].cstate == VR_ALIAS)
			v->alias[in->dst] = in->src1;
	}
	bytes = ((size_t)VERIFY_REGS + max_slot + 1) * sizeof(uint32_t);
	v->stamp = arena_alloc(ra->arena, bytes);
	v->owner = arena_alloc(ra->arena, bytes);
	memset(v->stamp, 0, bytes);
}

static void verify_claim (Verify *v, uint32_t r, uint32_t blk)
{
	Reg *reg = v->ra->regs + r;
	int8_t h = reg->home[blk];
	uint32_t root = v->alias[r];
	uint32_t key;

	if (reg->cstate == VR_CONST || reg->cstate == VR_GLOBAL || h == VR_NO_HOME)
		return;
	key = h >= 0 ? (uint32_t)h : VERIFY_REGS + reg->slot;
	if (v->stamp[key] == v->epoch && v->owner[key] != root)
		verify_fail(v, "two live vregs share the same home");
	v->stamp[key] = v->epoch;
	v->owner[key] = root;
}

static void verify_set (Verify *v, const uint64_t *set, uint32_t dst, uint32_t blk)
{
	v->epoch++;
	for (uint32_t w = 0; w < v->ra->words; w++) {
		uint64_t bits = set[w];

		while (bits) {
			verify_claim(v, (w << 6) + (uint32_t)__builtin_ctzll(bits), blk);
			bits &= bits - 1;
		}
	}
	if (dst != NO_REG) verify_claim(v, dst, blk);
}

static void verify_needs (Verify *v, IRInstr *in, uint32_t blk)
{
	RegAlloc *ra = v->ra;
	uint64_t mask = needs_mask(ra->ctx->arch, ra->regs, in);

	if (!mask) return;
	for (uint32_t w = 0; w < ra->words; w++) {
		uint64_t bits = ra->tmp[w];

		while (bits) {
			uint32_t r = (w << 6) + (uint32_t)__builtin_ctzll(bits);
			int8_t h = ra->regs[r].home[blk];

			bits &= bits - 1;
			if (h >= 0 && (mask >> h & 1))
				verify_fail(v, "live vreg sits in a register the instruction clobbers");
		}
	}
}

static void verify_block_live (Verify *v, uint32_t b)
{
	RegAlloc *ra = v->ra;
	CfgBlock *blk = ra->cfg.blocks + b;
	IRInstr *code = ra->ir->instrs + blk->start;
	uint32_t base = blk->start - ra->fn->start;

	if (!blk->count) return;
	cfg_live_out(&ra->cfg, b, ra->tmp, ra->words);
	for (uint32_t i = blk->count; i-- > 0;) {
		IRInstr *in = code + i;
		uint32_t part = ra->ctx->ins_block[base + i];
		uint32_t *src[3];
		int n;

		v->at = base + i;
		verify_set(v, ra->tmp, in->dst, part);
		if (in->dst != NO_REG)
			ra->tmp[in->dst >> 6] &= ~(1ull << (in->dst & 63));
		verify_needs(v, in, part);
		n = get_srcs(in, src);
		for (int k = 0; k < n; k++)
			ra->tmp[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	}
	verify_set(v, ra->tmp, NO_REG, ra->ctx->ins_block[base]);
}

static void verify_covered (Verify *v, uint32_t r, uint32_t from, uint32_t to, uint32_t first, uint32_t n)
{
	RegAlloc *ra = v->ra;
	Reg *reg = ra->regs + r;
	int8_t a = reg->home[from];
	int8_t b = reg->home[to];

	if (reg->cstate == VR_CONST || a == b) return;
	if (a == VR_NO_HOME || b == VR_NO_HOME) return;
	for (uint32_t k = 0; k < n; k++) {
		const EdgeMove *m = ra->ctx->moves + first + k;

		if (m->from == a && m->to == b) return;
	}
	verify_fail(v, "edge does not move a live-in vreg to its new home");
}

static void verify_edge (Verify *v, uint32_t from, uint32_t t, uint32_t first, uint32_t n)
{
	RegAlloc *ra = v->ra;
	CfgBlock *dst = ra->cfg.blocks + t;
	uint32_t to = ra->ctx->ins_block[dst->start - ra->fn->start];

	for (uint32_t k = 0; k < n; k++) {
		const EdgeMove *m = ra->ctx->moves + first + k;
		const Reg *reg = ra->regs + m->vreg;

		if (reg->home[from] != m->from || reg->home[to] != m->to)
			verify_fail(v, "edge move does not match the vreg homes");
	}
	if (from == to) return;
	for (uint32_t w = 0; w < ra->words; w++) {
		uint64_t bits = dst->live_in[w];

		while (bits) {
			verify_covered(v, (w << 6) + (uint32_t)__builtin_ctzll(bits),
					from, to, first, n);
			bits &= bits - 1;
		}
	}
}

static void verify_block_edges (Verify *v, uint32_t b)
{
	RegAlloc *ra = v->ra;
	CfgBlock *blk = ra->cfg.blocks + b;
	IRInstr *last = ra->ir->instrs + blk->start + blk->count - 1;
	uint32_t at = blk->start - ra->fn->start + blk->count - 1;
	const EdgeMoves *e = ra->ctx->edges + at;
	int fall = last->op != IR_JMP && last->op != IR_RET && b + 1 < ra->cfg.block_count;
	int jump = last->op == IR_JMP || last->op == IR_JZ || last->op == IR_JNZ;

	if (!blk->count) return;
	v->at = at;
	if (e->first + e->fall + e->taken > ra->ctx->move_count)
		verify_fail(v, "edge moves out of range");
	if ((!fall && e->fall) || (!jump && e->taken))
		verify_fail(v, "edge moves on an edge that does not exist");
	if (fall) verify_edge(v, ra->ctx->ins_block[at], b + 1, e->first, e->fall);
	if (!jump) return;

	uint32_t t = cfg_label_block(ra->ir, &ra->cfg, last->target);
	if (t != CFG_NO_BLOCK)
		verify_edge(v, ra->ctx->ins_block[at], t, e->first + e->fall, e->taken);
}

void verify_regalloc (RegAlloc *ra)
{
	Verify v = { .ra = ra };

	if (!ra->fn->reg_count) return;
	verify_vregs(&v);
	verify_instrs(&v);
	if (ra->spill_all) return;
	verify_setup(&v);
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		verify_block_live(&v, b);
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		verify_block_edges(&v, b);
}
