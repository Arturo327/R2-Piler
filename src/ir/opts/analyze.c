#include "ir/opts/common.h"

#include <string.h>

static void count_regs (IR *ir, OptFn *f)
{
	IRFn *fn = f->fn;

	memset(f->defs, 0, (size_t)fn->reg_count * sizeof(uint32_t));
	memset(f->uses, 0, (size_t)fn->reg_count * sizeof(uint32_t));

	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = ir->instrs + fn->start + j;

		if (in->op == IR_NOP) continue;
		if (in->dst != NO_REG) {
			f->defs[in->dst]++;
			f->def_at[in->dst] = fn->start + j;
		}

		uint32_t *src[3];
		int n = get_srcs(in, src);
		for (int k = 0; k < n; k++)
			f->uses[*src[k]]++;
	}
}

static void count_label_refs (IR *ir, OptFn *f)
{
	for (uint32_t j = 0; j < f->fn->count; j++) {
		IRInstr *in = ir->instrs + f->fn->start + j;

		if (in->op == IR_JMP || in->op == IR_JZ || in->op == IR_JNZ)
			f->lrefs[in->target]++;
	}
}

void analyze_fn (Optimizer *opt, OptFn *f)
{
	cfg_fill_blocks(opt->ir, f->fn, f->blocks, &f->block_count);
	opt_cfg(f, &opt->cfg);
	cfg_build_lmap(opt->ir, &opt->cfg);
	count_regs(opt->ir, f);
	count_label_refs(opt->ir, f);
}
