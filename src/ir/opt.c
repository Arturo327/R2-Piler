#include "ir/opt.h"

#include <string.h>

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

static uint32_t count_blocks (IR *ir, IRFn *fn)
{
	uint32_t n = 0;

	for (uint32_t i = 0; i < fn->count; i++) 
		if (starts_block(ir, fn, i)) n++;

	return n;
}

static void fill_blocks (IR *ir, OptFn *optfn, IRFn *fn)
{
	uint32_t b = 0;

	if (!optfn->block_count) return;
	optfn->blocks[0].start = fn->start;
	optfn->blocks[0].count = 0;

	for (uint32_t i = 0; i < fn->count; i++) {
		if (i > 0 && starts_block(ir, fn, i)) {
			b++;
			optfn->blocks[b].start = fn->start + i;
			optfn->blocks[b].count = 0;
		}
		optfn->blocks[b].count++;
	}
}

static void setup_fn (OptFn *optfn, IR *ir, IRFn *fn, uint32_t i)
{
	optfn->fn_idx = i;
	optfn->reg_count = fn->reg_count;
	optfn->label_cap = ir->label_count;
	optfn->block_count = count_blocks(ir, fn);
}

static void alloc_fn (Arena *a, OptFn *optfn, IR *ir, IRFn *fn)
{
	optfn->blocks = arena_alloc(a, optfn->block_count ?
			optfn->block_count * sizeof(OptBlock) : sizeof(OptBlock));
	optfn->lmap = arena_alloc(a, ir->label_count ?
			ir->label_count * sizeof(uint32_t) : sizeof(uint32_t));
	optfn->defs = arena_alloc(a, fn->reg_count ?
			fn->reg_count * sizeof(uint32_t) : sizeof(uint32_t));
	optfn->uses = arena_alloc(a, fn->reg_count ?
			fn->reg_count * sizeof(uint32_t) : sizeof(uint32_t));
}

static void clear_fn (OptFn *optfn, IR *ir, IRFn *fn)
{
	memset(optfn->lmap, 0xFF, ir->label_count ?
			ir->label_count * sizeof(uint32_t) : sizeof(uint32_t));
	memset(optfn->defs, 0, fn->reg_count ?
			fn->reg_count * sizeof(uint32_t) : sizeof(uint32_t));
	memset(optfn->uses, 0, fn->reg_count ?
			fn->reg_count * sizeof(uint32_t) : sizeof(uint32_t));
}

static void build_lmap (IR *ir, OptFn *optfn)
{
	for (uint32_t j = 0; j < optfn->block_count; j++) {
		IRInstr *in = ir->instrs + optfn->blocks[j].start;
		if (in->op != IR_LABEL || in->target >= ir->label_count) continue;
		if (optfn->lmap[in->target] != OPT_NO_BLOCK) continue;
		optfn->lmap[in->target] = j;
	}
}

static void count_regs (IR *ir, OptFn *optfn, IRFn *fn)
{
	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = ir->instrs + fn->start + j;
		if (in->op == IR_NOP) continue;
		if (in->dst != NO_REG && in->dst < fn->reg_count) optfn->defs[in->dst]++;
		if (in->src1 != NO_REG && in->src1 < fn->reg_count) optfn->uses[in->src1]++;
		if (in->src2 != NO_REG && in->src2 < fn->reg_count) optfn->uses[in->src2]++;
	}
}

static void analyze_fn (Optimizer *opt, uint32_t i)
{
	OptFn *optfn = opt->fns + i;
	IR *ir = opt->ir;
	IRFn *fn = ir->fns + i;

	setup_fn(optfn, ir, fn, i);
	alloc_fn(opt->arena, optfn, ir, fn);
	fill_blocks(ir, optfn, fn);
	clear_fn(optfn, ir, fn);
	build_lmap(ir, optfn);
	count_regs(ir, optfn, fn);
}

static void init_opt (Optimizer *opt, IR *ir, Arena *a, OptLevel level)
{
	opt->arena = a;
	opt->ir = ir;
	opt->level = level;
	opt->fn_count = ir->fn_count;
	opt->fns = arena_alloc(a, ir->fn_count ?
			ir->fn_count * sizeof(OptFn) : sizeof(OptFn));

	for (uint32_t i = 0; i < ir->fn_count; i++)
		analyze_fn(opt, i);
}

void optimize_ir (IR *ir, Arena *a, OptLevel opt_level)
{
	Optimizer opt;
	init_opt(&opt, ir, a, opt_level);
}
