#include "ir/opts/common.h"

#include <string.h>

int opt_dce (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = f->fn->count; i-- > 0;) {
		IRInstr *in = code + i;

		if (in->dst == NO_REG || f->uses[in->dst] || !is_deletable[in->op]) continue;
		if (in->src1 != NO_REG) f->uses[in->src1]--;
		if (in->src2 != NO_REG) f->uses[in->src2]--;
		kill_instr(in);
		changed = 1;
	}
	return changed;
}

static inline int get_srcs (IRInstr *in, uint32_t *out[3])
{
	int n = 0;

	if (in->src1 != NO_REG) out[n++] = &in->src1;
	if (in->src2 != NO_REG) out[n++] = &in->src2;
	if (in->op == IR_SELECT) out[n++] = &in->cond;
	return n;
}

static int live_step (IRInstr *in, uint64_t *live)
{
	uint32_t *src[3];
	int n = get_srcs(in, src);

	if (in->dst != NO_REG) {
		uint64_t bit = 1ull << (in->dst & 63);

		if (!(live[in->dst >> 6] & bit) && is_deletable[in->op]) return 1;
		live[in->dst >> 6] &= ~bit;
	}
	for (int k = 0; k < n; k++)
		live[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	return 0;
}

static void live_out (OptFn *f, OptBlock *blk, uint32_t words)
{
	memset(f->live_tmp, 0, (size_t)words * sizeof(uint64_t));
	for (uint32_t k = 0; k < blk->succ_count; k++) {
		uint64_t *src = f->blocks[blk->succ[k]].live_in;

		for (uint32_t w = 0; w < words; w++)
			f->live_tmp[w] |= src[w];
	}
}

static int live_block (Optimizer *opt, OptFn *f, OptBlock *blk, uint32_t words)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	live_out(f, blk, words);
	for (uint32_t i = blk->count; i-- > 0;)
		live_step(code + i, f->live_tmp);
	for (uint32_t w = 0; w < words; w++) {
		changed |= f->live_tmp[w] != blk->live_in[w];
		blk->live_in[w] = f->live_tmp[w];
	}
	return changed;
}

static int live_sweep (Optimizer *opt, OptFn *f, OptBlock *blk, uint32_t words)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	live_out(f, blk, words);
	for (uint32_t i = blk->count; i-- > 0;) {
		if (!live_step(code + i, f->live_tmp)) continue;
		kill_instr(code + i);
		changed = 1;
	}
	return changed;
}

static void live_setup (Optimizer *opt, OptFn *f, uint32_t words)
{
	size_t need = (size_t)f->block_count * words;

	if (need > f->live_cap) {
		f->live = arena_alloc(opt->arena, need * sizeof(uint64_t));
		f->live_cap = need;
	}
	memset(f->live, 0, need * sizeof(uint64_t));

	uint64_t *p = f->live;
	for (uint32_t b = 0; b < f->block_count; b++, p += words)
		f->blocks[b].live_in = p;
}

static void live_solve (Optimizer *opt, OptFn *f, uint32_t words)
{
	int again = 1;

	while (again) {
		again = 0;
		for (uint32_t b = f->block_count; b-- > 0;)
			again |= live_block(opt, f, f->blocks + b, words);
	}
}

int opt_live_dce (Optimizer *opt, OptFn *f)
{
	uint32_t words = (f->fn->reg_count + 63) >> 6;
	int changed = 0;

	if (!words) return 0;
	build_cfg(opt->ir, f);
	live_setup(opt, f, words);
	live_solve(opt, f, words);
	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= live_sweep(opt, f, f->blocks + b, words);
	return changed;
}
