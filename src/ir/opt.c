#include "ir/opt.h"
#include "ir/opts/common.h"

#include <string.h>

#define OPT_MAX_ROUNDS 8

typedef int (*OptPass) (Optimizer *opt, OptFn *f);

typedef struct OptPassDesc {
	OptPass run;
	OptLevel min_level;
	uint8_t fresh;
} OptPassDesc;

static const OptPassDesc passes[] = {
	{ opt_propagate, OPT_BASIC, 1 },
	{ opt_fold, OPT_BASIC, 1 },
	{ opt_disp, OPT_BASIC, 0 },
	{ opt_frame, OPT_BASIC, 0 },
	{ opt_fields, OPT_BASIC, 0 },
	{ opt_cse, OPT_FULL, 1 },
	{ opt_coalesce, OPT_BASIC, 1 },
	{ opt_dce, OPT_BASIC, 1 },
	{ opt_live_dce, OPT_FULL, 0 },
	{ opt_unreachable, OPT_BASIC, 0 },
	{ opt_jumps, OPT_BASIC, 0 },
	{ opt_strength, OPT_BASIC, 0 },
	{ opt_globals, OPT_BASIC, 0 },
	{ opt_dse, OPT_BASIC, 0 },
};

void optimize_fn (Optimizer *opt, OptFn *f)
{
	size_t n = sizeof(passes) / sizeof(passes[0]);
	ArenaMark mark = arena_mark(opt->arena);

	for (int round = 0; round < OPT_MAX_ROUNDS; round++) {
		int changed = 0;
		int dirty = 0;

		analyze_fn(opt, f);
		for (size_t p = 0; p < n; p++) {
			if (passes[p].min_level > opt->level) continue;
			if (dirty && passes[p].fresh) analyze_fn(opt, f);
			dirty = passes[p].run(opt, f);
			changed |= dirty;
		}
		if (!changed) break;
	}
	arena_release(opt->arena, &mark);
	f->live = NULL;
	f->live_cap = 0;
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

static void *grow_buf (Arena *a, void *ptr, size_t *cap, size_t need, size_t elem)
{
	size_t old = *cap;
	size_t n = old ? old : 1;

	if (need <= old) return ptr;
	while (n < need) n <<= 1;
	void *out = arena_realloc(a, ptr, old * elem, n * elem);
	*cap = n;
	return out;
}

static void init_caches (Optimizer *opt, OptFn *f, IR *ir, Arena *a, size_t max_regs)
{
	size_t globals = (size_t)ir->global_count + 1;
	size_t words = ((max_regs + 63) >> 6) + 1;

	f->gval = grow_buf(a, f->gval, &opt->cap.gval, globals, sizeof(uint32_t));
	f->gstamp = grow_buf(a, f->gstamp, &opt->cap.gstamp, globals, sizeof(uint32_t));
	f->dstamp = grow_buf(a, f->dstamp, &opt->cap.dstamp, max_regs, sizeof(uint32_t));
	memset(f->gstamp, 0, globals * sizeof(uint32_t));
	memset(f->dstamp, 0, max_regs * sizeof(uint32_t));
	f->gepoch = 1;
	f->depoch = 1;
	if (!opt->gvis) {
		f->gvis = arena_alloc(a, globals * sizeof(uint32_t));
		f->gvis_count = visible_globals(ir, a, f->gvis);
		opt->gvis = f->gvis;
		opt->gvis_count = f->gvis_count;
	} else {
		f->gvis = opt->gvis;
		f->gvis_count = opt->gvis_count;
	}

	f->live = NULL;
	f->live_cap = 0;
	f->live_tmp = grow_buf(a, f->live_tmp, &opt->cap.live_tmp, words, sizeof(uint64_t));
	if (!f->cse) f->cse = arena_alloc(a, OPT_CSE_MAX * sizeof(CseEntry));
	f->cse_count = 0;
	if (!f->fwd) f->fwd = arena_alloc(a, OPT_FWD_MAX * sizeof(FwdEntry));
	f->fwd_count = 0;
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
	opt->lmap = grow_buf(a, opt->lmap, &opt->cap.lmap, labels, sizeof(uint32_t));
	opt->lrefs = grow_buf(a, opt->lrefs, &opt->cap.lrefs, labels, sizeof(uint32_t));
	memset(opt->lmap, 0xFF, labels * sizeof(uint32_t));

	opt->cur.lmap = opt->lmap;
	opt->cur.lrefs = opt->lrefs;
	opt->cur.defs = grow_buf(a, opt->cur.defs, &opt->cap.defs, max_regs, sizeof(uint32_t));
	opt->cur.uses = grow_buf(a, opt->cur.uses, &opt->cap.uses, max_regs, sizeof(uint32_t));
	opt->cur.def_at = grow_buf(a, opt->cur.def_at, &opt->cap.def_at, max_regs, sizeof(uint32_t));
	opt->cur.blocks = grow_buf(a, opt->cur.blocks, &opt->cap.blocks, max_instrs, sizeof(CfgBlock));
	opt->cur.work = grow_buf(a, opt->cur.work, &opt->cap.work, max_instrs, sizeof(uint32_t));
	opt->cur.pred_cnt = grow_buf(a, opt->cur.pred_cnt, &opt->cap.pred_cnt, max_instrs + 1, sizeof(uint32_t));
	opt->cur.pred_start = grow_buf(a, opt->cur.pred_start, &opt->cap.pred_start, max_instrs + 2, sizeof(uint32_t));
	opt->cur.preds = grow_buf(a, opt->cur.preds, &opt->cap.preds, max_instrs * 2 + 8, sizeof(uint32_t));
	opt->cur.pred_cap = (uint32_t)opt->cap.preds;
	opt->cur.block_cap = (uint32_t)opt->cap.work;
	opt->cur.queued = grow_buf(a, opt->cur.queued, &opt->cap.queued, max_instrs + 1, 1);

	opt->cur.kstamp = grow_buf(a, opt->cur.kstamp, &opt->cap.kstamp, max_regs, sizeof(uint32_t));
	opt->cur.kval = grow_buf(a, opt->cur.kval, &opt->cap.kval, max_regs, sizeof(int64_t));
	memset(opt->cur.kstamp, 0, max_regs * sizeof(uint32_t));
	opt->cur.epoch = 1;

	init_caches(opt, &opt->cur, ir, a, max_regs);
}

static void optimize_all (Optimizer *opt)
{
	if (opt->level < OPT_BASIC) return;
	for (uint32_t i = 0; i < opt->ir->fn_count; i++) {
		if (opt->ir->fns[i].is_extern) continue;
		opt->cur.fn = opt->ir->fns + i;
		optimize_fn(opt, &opt->cur);
	}
}

void optimize_ir (IR *ir, Arena *a, OptLevel level)
{
	Optimizer opt = {0};

	if (level == NO_OPT) return;
	sroa_ir(ir, a);
	init_opt(&opt, ir, a, level);
	optimize_all(&opt);

	if (level == OPT_FULL) {
		compact_ir(ir);
		if (inline_ir(ir, a)) {
			sroa_ir(ir, a);
			init_opt(&opt, ir, a, level);
			optimize_all(&opt);
		}
	}

	run_static_init(&opt);
	compact_ir(ir);
}
