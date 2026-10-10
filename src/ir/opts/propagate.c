#include "ir/opts/common.h"

static int forward_copy (IRInstr *code, uint32_t n, IRInstr *mv, uint32_t pending)
{
	int changed = 0;

	for (uint32_t k = 0; k < n && pending; k++) {
		IRInstr *in = code + k;
		uint32_t *src[3];
		int cnt;

		if (in->op == IR_NOP) continue;
		cnt = get_srcs(in, src);
		for (int j = 0; j < cnt; j++) {
			if (*src[j] != mv->dst) continue;
			*src[j] = mv->src1;
			changed = 1;
			pending--;
		}
		if (in->dst == mv->src1) break;
	}
	return changed;
}

int opt_propagate (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++) {
		IRInstr *code = opt->ir->instrs + f->blocks[b].start;
		uint32_t n = f->blocks[b].count;

		for (uint32_t i = 0; i < n; i++) {
			IRInstr *mv = code + i;

			if (mv->op != IR_MOVE || f->defs[mv->dst] != 1) continue;
			if (mv->dst == mv->src1 || !same_rep(f->fn, mv->dst, mv->src1)) continue;
			changed |= forward_copy(code + i + 1, n - i - 1, mv, f->uses[mv->dst]);
		}
	}
	return changed;
}
