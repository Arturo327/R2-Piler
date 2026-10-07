#include "ir/opt.h"

#include <string.h>

#define OPT_MAX_ROUNDS 8

typedef int (*OptPass) (Optimizer *opt, OptFn *f);

typedef struct OptPassDesc {
	OptPass run;
	OptLevel min_level;
} OptPassDesc;

static int starts_block (IR *ir, IRFn *fn, uint32_t i)
{
	if (!i) return 1;

	uint8_t prev = ir->instrs[fn->start + i - 1].op;
	if (prev == IR_JMP || prev == IR_JZ || prev == IR_JNZ || prev == IR_RET)
		return 1;

	uint8_t cur = ir->instrs[fn->start + i].op;
	if (cur == IR_LABEL) return 1;
	return 0;
}

static void fill_blocks (IR *ir, OptFn *f)
{
	uint32_t b = 0;

	f->blocks[0].start = f->fn->start;
	f->blocks[0].count = 0;
	for (uint32_t i = 0; i < f->fn->count; i++) {
		if (i > 0 && starts_block(ir, f->fn, i)) {
			b++;
			f->blocks[b].start = f->fn->start + i;
			f->blocks[b].count = 0;
		}
		f->blocks[b].count++;
	}
	f->block_count = b + 1;
}

static void build_lmap (IR *ir, OptFn *f)
{
	for (uint32_t j = 0; j < f->block_count; j++) {
		IRInstr *in = ir->instrs + f->blocks[j].start;

		if (in->op != IR_LABEL) continue;
		f->lmap[in->target] = j;
		f->lrefs[in->target] = 0;
	}
}

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
		if (in->src1 != NO_REG)
			f->uses[in->src1]++;
		if (in->src2 != NO_REG)
			f->uses[in->src2]++;
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

static void analyze_fn (Optimizer *opt, OptFn *f)
{
	fill_blocks(opt->ir, f);
	build_lmap(opt->ir, f);
	count_regs(opt->ir, f);
	count_label_refs(opt->ir, f);
}

static const OptPassDesc passes[] = {
//	{ opt_propagate, OPT_BASIC },
//	{ opt_fold, OPT_BASIC },
//	{ opt_coalesce, OPT_FULL },
//	{ opt_dce, OPT_BASIC },
//	{ opt_unreachable, OPT_BASIC },
//	{ opt_jumps, OPT_BASIC }
};

static void optimize_fn (Optimizer *opt, OptFn *f)
{
	size_t n = sizeof(passes) / sizeof(passes[0]);

	for (int round = 0; round < OPT_MAX_ROUNDS; round++) {
		int changed = 0;

		for (size_t p = 0; p < n; p++) {
			if (passes[p].min_level > opt->level) continue;
			analyze_fn(opt, f);
			changed |= passes[p].run(opt, f);
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
	compact_ir(ir);
}
