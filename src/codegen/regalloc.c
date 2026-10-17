#include "codegen/regalloc.h"
#include "ir/cfg.h"

#include <string.h>

static void reset_regalloc (IRFn *fn, Reg *regs)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		regs[r].uses = 0;
		regs[r].slot = 0;
		regs[r].cval = 0;
		regs[r].cstate = VR_NONE;
		regs[r].defs = 0;
		regs[r].spill = 0;
	}
}

static void count_defs (IR *ir, IRFn *fn, Reg *regs)
{
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t *src[3];

		if (in->op == IR_NOP) continue;
		int n = get_srcs(in, src);

		for (int j = 0; j < n; j++)
			regs[*src[j]].uses++;
		if (in->dst != NO_REG && regs[in->dst].defs < 2)
			regs[in->dst].defs++;
	}
}

static void handle_alias (IRFn *fn, Reg *regs, IRInstr *in, int spill_all)
{
	uint32_t d = in->dst;
	uint32_t s = in->src1;

	if (in->op != IR_MOVE || !same_rep(fn, d, s)) return;
	if (regs[s].cstate == VR_CONST) {
		regs[d].cstate = VR_CONST;
		regs[d].cval = regs[s].cval;
		return;
	}
	if (spill_all) return;

	if (s >= fn->param_count || regs[s].cstate != VR_MEM) return;
	if (regs[s].defs != 1) return;

	regs[d].cstate = VR_ALIAS;
	regs[d].slot = s;
}

static void clasf_regs (IR *ir, IRFn *fn, Reg *regs, int spill_all)
{
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_NOP || in->dst == NO_REG) continue;
		regs[in->dst].cstate = VR_MEM;
		if (regs[in->dst].defs != 1) continue;

		if (in->op == IR_CONST) {
			regs[in->dst].cstate = VR_CONST;
			regs[in->dst].cval = in->imm64;
			continue;
		}

		regs[in->dst].cstate = VR_MEM;
		handle_alias(fn, regs, in, spill_all);
	}
}

static void alloc_homes (Arena *arena, IRFn *fn, Reg *regs, uint32_t block_count)
{
	if (!fn->reg_count) return;
	int8_t *homes = arena_alloc(arena, (size_t)fn->reg_count * block_count);
	for (uint32_t r = 0; r < fn->reg_count; r++)
		regs[r].home = homes + (size_t)r * block_count;
}

static void set_home (Reg *reg, uint32_t block_count, int h)
{
	for (uint32_t b = 0; b < block_count; b++)
		reg->home[b] = (int8_t)h;
}

static void assign_homes (IRFn *fn, Reg *regs, uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate == VR_CONST || regs[r].cstate == VR_ALIAS) {
			set_home(regs + r, block_count, VR_NO_HOME);
			continue;
		}
		if (!regs[r].uses) {
			set_home(regs + r, block_count, VR_NO_HOME);
			continue;
		}
		set_home(regs + r, block_count, VR_SPILL);
		regs[r].spill = 1;
	}
}

static int is_spilled (Reg *reg, uint32_t block_count)
{
	for (uint32_t b = 0; b < block_count; b++)
		if (reg->home[b] == VR_SPILL) return 1;
	return 0;
}

static uint32_t lay_size (IRFn *fn, Reg *regs, uint32_t c, uint8_t s,
		uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (!is_spilled(regs + r, block_count)) continue;
		if (types[fn->reg_types[r]].size != s) continue;
		c += s;
		regs[r].slot = c;
	}
	return c;
}

static void lay_sizes (IRFn *fn, Reg *regs, uint32_t block_count)
{
	uint32_t c = 0;
	c = lay_size(fn, regs, c, 8, block_count);
	c = lay_size(fn, regs, c, 4, block_count);
	c = lay_size(fn, regs, c, 2, block_count);
	lay_size(fn, regs, c, 1, block_count);
}

static void make_ctx (IRFn *fn, RegCtx *ctx, uint32_t reg_num)
{
	uint16_t all = (uint16_t)((1u << reg_num) - 1);

	for (uint32_t i = 0; i < fn->count; i++) {
		ctx->ins_block[i] = 0;
		ctx->save[i] = 0;
		ctx->free_regs[i] = all & (uint16_t)~ctx->callee_mask;
	}
	ctx->block_count = 1;
	ctx->callee_used = 0;
}

static void resolve (IRFn *fn, Reg *regs, uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate != VR_ALIAS) continue;
		uint32_t t = regs[r].slot;
		regs[r].slot = regs[t].slot;
		for (uint32_t b = 0; b < block_count; b++)
			regs[r].home[b] = regs[t].home[b];
	}
}

typedef struct RegAlloc {
	Arena *arena;
	IR *ir;
	IRFn *fn;
	Reg *regs;
	RegCtx *ctx;

	CfgBlock *blocks;
	uint32_t *lmap;
	uint32_t *lrefs;
	uint64_t *live;
	uint64_t *tmp;

	Cfg cfg;
	uint16_t all;
	uint32_t words;
} RegAlloc;

static void alloc_regalloc (RegAlloc *ra)
{
	uint32_t count = ra->fn->count ? ra->fn->count : 1;
	uint32_t labels = ra->ir->label_count + 1;

	ra->blocks = arena_alloc(ra->arena, (size_t)count * sizeof(CfgBlock));
	ra->lmap = arena_alloc(ra->arena, (size_t)labels * sizeof(uint32_t));
	ra->lrefs = arena_alloc(ra->arena, (size_t)labels * sizeof(uint32_t));
	ra->live = arena_alloc(ra->arena,
			(size_t)count * ra->words * sizeof(uint64_t));
	ra->tmp = arena_alloc(ra->arena, (size_t)ra->words * sizeof(uint64_t));
	memset(ra->lmap, 0xFF, (size_t)labels * sizeof(uint32_t));
}

static void build_cfg (RegAlloc *ra)
{
	Cfg *cfg = &ra->cfg;

	cfg->blocks = ra->blocks;
	cfg->lmap = ra->lmap;
	cfg->lrefs = ra->lrefs;
	cfg->work = NULL;
	cfg_fill_blocks(ra->ir, ra->fn, cfg->blocks, &cfg->block_count);
	cfg_build_lmap(ra->ir, cfg);
	cfg_build_edges(ra->ir, cfg);
	cfg_live_solve(ra->ir, cfg, ra->live, ra->tmp, ra->words);
}

static uint16_t homes_word (RegAlloc *ra, uint64_t bits, uint32_t w, uint32_t blk)
{
	uint16_t m = 0;

	while (bits) {
		uint32_t r = (w << 6) + (uint32_t)__builtin_ctzll(bits);
		int h = ra->regs[r].home[blk];

		if (h >= 0) m |= (uint16_t)(1u << h);
		bits &= bits - 1;
	}
	return m;
}

static uint16_t homes_live (RegAlloc *ra, const uint64_t *live, uint32_t blk)
{
	uint16_t m = 0;
	for (uint32_t w = 0; w < ra->words; w++)
		m |= homes_word(ra, live[w], w, blk);
	return m;
}

static uint16_t callee_homes (RegAlloc *ra)
{
	uint16_t m = 0;

	for (uint32_t r = 0; r < ra->fn->reg_count; r++) {
		for (uint32_t b = 0; b < ra->ctx->block_count; b++) {
			int h = ra->regs[r].home[b];
			if (h >= 0) m |= (uint16_t)(1u << h);
		}
	}
	return m & ra->ctx->callee_mask;
}

static void sweep_block (RegAlloc *ra, uint32_t b)
{
	CfgBlock *blk = ra->cfg.blocks + b;
	IRInstr *code = ra->ir->instrs + blk->start;
	uint32_t base = blk->start - ra->fn->start;
	uint16_t caller = (uint16_t)~ra->ctx->callee_mask;
	uint16_t allowed = (uint16_t)(caller | ra->ctx->callee_used);

	cfg_live_out(&ra->cfg, b, ra->tmp, ra->words);
	for (uint32_t i = blk->count; i-- > 0;) {
		IRInstr *in = code + i;
		uint32_t at = base + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		if (in->dst != NO_REG)
			ra->tmp[in->dst >> 6] &= ~(1ull << (in->dst & 63));
		uint16_t hm = homes_live(ra, ra->tmp, ra->ctx->ins_block[at]);
		ra->ctx->free_regs[at] = ra->all & (uint16_t)~hm & allowed;

		if (in->op == IR_CALL) ra->ctx->save[at] = hm & caller;
		for (int k = 0; k < n; k++)
			ra->tmp[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	}
}

static void sweep_fn (RegAlloc *ra)
{
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		sweep_block(ra, b);
}

static void fill_ctx (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, uint32_t reg_num)
{
	uint32_t words = (fn->reg_count + 63) >> 6;
	if (!words) return;

	RegAlloc ra = {0};
	ra.arena = arena;
	ra.ir = ir;
	ra.fn = fn;
	ra.regs = regs;
	ra.ctx = ctx;

	ra.words = words;
	ra.all = (uint16_t)((1u << reg_num) - 1);

	alloc_regalloc(&ra);
	build_cfg(&ra);
	ra.ctx->callee_used = callee_homes(&ra);
	sweep_fn(&ra);
}

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs,
		RegCtx *ctx, uint32_t reg_num, int spill_all)
{
	reset_regalloc(fn, regs);
	make_ctx(fn, ctx, reg_num);
	alloc_homes(arena, fn, regs, ctx->block_count);

	count_defs(ir, fn, regs);
	clasf_regs(ir, fn, regs, spill_all);

	assign_homes(fn, regs, ctx->block_count);
	lay_sizes(fn, regs, ctx->block_count);

	resolve(fn, regs, ctx->block_count);
	fill_ctx(arena, ir, fn, regs, ctx, reg_num);
}
