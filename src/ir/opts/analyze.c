#include "ir/opts/common.h"

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

void analyze_fn (Optimizer *opt, OptFn *f)
{
	fill_blocks(opt->ir, f);
	build_lmap(opt->ir, f);
	count_regs(opt->ir, f);
	count_label_refs(opt->ir, f);
}

uint32_t label_block (IR *ir, OptFn *f, uint32_t label)
{
	uint32_t b = f->lmap[label];
	IRInstr *in;

	if (b >= f->block_count) return OPT_NO_BLOCK;
	in = ir->instrs + f->blocks[b].start;
	return in->op == IR_LABEL && in->target == label ? b : OPT_NO_BLOCK;
}

void build_cfg (IR *ir, OptFn *f)
{
	for (uint32_t b = 0; b < f->block_count; b++) {
		OptBlock *blk = f->blocks + b;
		IRInstr *last = ir->instrs + blk->start + blk->count - 1;
		int jumps = last->op == IR_JMP || last->op == IR_JZ || last->op == IR_JNZ;
		int falls = last->op != IR_JMP && last->op != IR_RET && b + 1 < f->block_count;

		blk->succ_count = 0;
		blk->reachable = 0;
		if (falls) blk->succ[blk->succ_count++] = b + 1;
		if (!jumps) continue;

		uint32_t t = label_block(ir, f, last->target);
		if (t != OPT_NO_BLOCK) blk->succ[blk->succ_count++] = t;
	}
}

void mark_reachable (OptFn *f)
{
	uint32_t sp = 0;

	f->work[sp++] = 0;
	f->blocks[0].reachable = 1;
	while (sp) {
		OptBlock *b = f->blocks + f->work[--sp];

		for (uint32_t k = 0; k < b->succ_count; k++) {
			if (f->blocks[b->succ[k]].reachable) continue;
			f->blocks[b->succ[k]].reachable = 1;
			f->work[sp++] = b->succ[k];
		}
	}
}
