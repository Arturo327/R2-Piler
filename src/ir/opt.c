#include "ir/opt.h"
#include "ir/opts/common.h"

#include <string.h>

#define OPT_MAX_ROUNDS 8
#define OPT_MAX_HOPS 8

typedef int (*OptPass) (Optimizer *opt, OptFn *f);

typedef struct OptPassDesc {
	OptPass run;
	OptLevel min_level;
} OptPassDesc;

static const OptPassDesc passes[] = {
	{ opt_propagate, OPT_BASIC },
	{ opt_fold, OPT_BASIC },
	{ opt_cse, OPT_FULL },
	{ opt_coalesce, OPT_BASIC },
	{ opt_dce, OPT_BASIC },
	{ opt_live_dce, OPT_FULL },
	{ opt_unreachable, OPT_BASIC },
	{ opt_jumps, OPT_BASIC },
	{ opt_strength, OPT_BASIC },
	{ opt_globals, OPT_BASIC },
	{ opt_dse, OPT_BASIC },
};

void optimize_fn (Optimizer *opt, OptFn *f)
{
	size_t n = sizeof(passes) / sizeof(passes[0]);
	int dirty = 1;

	for (int round = 0; round < OPT_MAX_ROUNDS; round++) {
		int changed = 0;

		for (size_t p = 0; p < n; p++) {
			if (passes[p].min_level > opt->level) continue;
			if (dirty) analyze_fn(opt, f);
			dirty = passes[p].run(opt, f);
			changed |= dirty;
		}
		if (!changed) break;
	}
}

static void compact_ir (IR *ir)
{
	uint32_t out = 0;

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		IRFn *fn = ir->fns + i;
		uint32_t begin = fn->start;
		uint32_t end = begin + fn->count;

		fn->start = out;
		for (uint32_t j = begin; j < end; j++)
			if (ir->instrs[j].op != IR_NOP) ir->instrs[out++] = ir->instrs[j];
		fn->count = out - fn->start;
	}
	ir->instr_count = out;
}

static void init_caches (OptFn *f, IR *ir, Arena *a, size_t max_regs)
{
	size_t globals = (size_t)ir->global_count + 1;

	f->gval = arena_alloc(a, globals * sizeof(uint32_t));
	f->gstamp = arena_alloc(a, globals * sizeof(uint32_t));
	f->dstamp = arena_alloc(a, max_regs * sizeof(uint32_t));
	memset(f->gstamp, 0, globals * sizeof(uint32_t));
	memset(f->dstamp, 0, max_regs * sizeof(uint32_t));
	f->gepoch = 1;
	f->depoch = 1;

	f->live = NULL;
	f->live_cap = 0;
	f->live_tmp = arena_alloc(a, (((max_regs + 63) >> 6) + 1) * sizeof(uint64_t));
	f->cse = arena_alloc(a, OPT_CSE_MAX * sizeof(CseEntry));
	f->cse_count = 0;
}

static void init_opt (Optimizer *opt, IR *ir, Arena *a, OptLevel level)
{
	size_t max_regs = 1;
	size_t max_instrs = 1;
	size_t labels = (size_t)ir->label_count + 1;

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (ir->fns[i].reg_count > max_regs) max_regs = ir->fns[i].reg_count;
		if (ir->fns[i].count > max_instrs) max_instrs = ir->fns[i].count;
	}
	opt->arena = a;
	opt->ir = ir;
	opt->level = level;
	opt->lmap = arena_alloc(a, labels * sizeof(uint32_t));
	opt->lrefs = arena_alloc(a, labels * sizeof(uint32_t));
	memset(opt->lmap, 0xFF, labels * sizeof(uint32_t));

	opt->cur.lmap = opt->lmap;
	opt->cur.lrefs = opt->lrefs;
	opt->cur.defs = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.uses = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.def_at = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.blocks = arena_alloc(a, max_instrs * sizeof(OptBlock));
	opt->cur.work = arena_alloc(a, max_instrs * sizeof(uint32_t));

	opt->cur.kstamp = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.kval = arena_alloc(a, max_regs * sizeof(int64_t));
	memset(opt->cur.kstamp, 0, max_regs * sizeof(uint32_t));
	opt->cur.epoch = 1;

	init_caches(&opt->cur, ir, a, max_regs);
}

void optimize_ir (IR *ir, Arena *a, OptLevel level)
{
	Optimizer opt;

	if (level == NO_OPT) return;
	init_opt(&opt, ir, a, level);
	for (uint32_t i = 0; i < ir->fn_count; i++) {
		opt.cur.fn = ir->fns + i;
		optimize_fn(&opt, &opt.cur);
	}
	run_static_init(&opt);
	compact_ir(ir);
}
