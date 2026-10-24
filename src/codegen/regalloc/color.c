#include "codegen/regalloc/common.h"

#include <string.h>

#define LOOP_WEIGHT 8u

static int tail_self (ColorRegs *cr, uint32_t i)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	IRInstr *in = ir->instrs + fn->start + i;
	IRInstr *end = ir->instrs + fn->start + fn->count;
	IRInstr *n = in + 1;
	uint8_t from = ir->fns[in->target].ret_type;
	uint8_t to = fn->ret_type;

	while (n < end && n->op == IR_LABEL) n++;
	if (n >= end || n->op != IR_RET) return 0;
	if (n->src1 != in->dst || in->argc > 6) return 0;
	if (in->dst != NO_REG && cr->regs[in->dst].uses != 1) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

static void loop_weights (ColorRegs *cr)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	uint32_t labels = ir->label_count + 1;
	uint32_t *lpos = arena_alloc(cr->arena, labels * sizeof(uint32_t));
	uint32_t self = (uint32_t)(fn - ir->fns);

	for (uint32_t i = 0; i < labels; i++)
		lpos[i] = UINT32_MAX;
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		if (in->op == IR_LABEL && in->target < labels)
			lpos[in->target] = i;
	}

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t t = UINT32_MAX;

		if (in->op == IR_JMP || in->op == IR_JZ || in->op == IR_JNZ)
			t = in->target < labels ? lpos[in->target] : UINT32_MAX;
		else if (in->op == IR_CALL && in->target == self && tail_self(cr, i))
			t = 0;

		if (t == UINT32_MAX || t > i) continue;
		cr->w[t]++;
		if (i + 1 < fn->count) cr->w[i + 1]--;
	}

	int16_t d = 0;
	for (uint32_t i = 0; i < fn->count; i++) {
		d = (int16_t)(d + cr->w[i]);
		cr->w[i] = d > 0 ? LOOP_WEIGHT : 1;
	}
}

static void weigh_regs (ColorRegs *cr)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		for (int k = 0; k < n; k++)
			cr->regs[*src[k]].wscore += (uint32_t)cr->w[i];
		if (in->dst != NO_REG)
			cr->regs[in->dst].wscore += (uint32_t)cr->w[i];
	}
}

static void mark_weighted (ColorRegs *cr)
{
	uint32_t count = cr->fn->count ? cr->fn->count : 1;
	cr->w = arena_alloc(cr->arena, count * sizeof(int16_t));
	memset(cr->w, 0, count * sizeof(int16_t));
	loop_weights(cr);
	weigh_regs(cr);
}

void color_graph (Graph *g)
{
	uint32_t blocks = g->cr->ctx->block_count;

	for (uint32_t i = 0; i < blocks; i++) {
		g->blk = g->order[i];
		build_graph(g);
		if (g->node_num && g->k) paint_graph(g);
	}
}

static void color_run (ColorRegs *cr)
{
	if (!cr->fn->reg_count) return;
	mark_weighted(cr);

	Graph graph;
	make_graph(cr, &graph);
	color_graph(&graph);
}

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg, uint32_t reg_num)
{
	ColorRegs cr = {
		.arena = arena,
		.ir = ir,
		.fn = fn,
		.regs = regs,
		.ctx = ctx,
		.cfg = cfg,
		.num_regs = reg_num,
	};
	color_run(&cr);
}
