#include "codegen/regalloc/common.h"

#include <string.h>

#define LOOP_SHIFT 3u
#define LOOP_MAX_DEPTH 4

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

static uint32_t *label_positions (ColorRegs *cr)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	uint32_t labels = ir->label_count + 1;
	uint32_t *lpos = arena_alloc(cr->arena, labels * sizeof(uint32_t));

	memset(lpos, 0xFF, labels * sizeof(uint32_t));
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_LABEL && in->target < labels)
			lpos[in->target] = i;
	}
	return lpos;
}

static uint32_t back_target (ColorRegs *cr, const uint32_t *lpos, uint32_t i)
{
	IR *ir = cr->ir;
	IRFn *fn = cr->fn;
	IRInstr *in = ir->instrs + fn->start + i;
	uint32_t t = UINT32_MAX;

	if (in->op == IR_JMP || in->op == IR_JZ || in->op == IR_JNZ) {
		if (in->target <= ir->label_count) t = lpos[in->target];
	} else if (in->op == IR_CALL && in->target == (uint32_t)(fn - ir->fns)
			&& tail_self(cr, i)) {
		t = 0;
	}
	return t <= i ? t : UINT32_MAX;
}

static uint32_t *loop_ends (ColorRegs *cr)
{
	uint32_t count = cr->fn->count;
	uint32_t *lpos = label_positions(cr);
	uint32_t *end = arena_alloc(cr->arena, count * sizeof(uint32_t));

	memset(end, 0xFF, count * sizeof(uint32_t));
	for (uint32_t i = 0; i < count; i++) {
		uint32_t t = back_target(cr, lpos, i);

		if (t != UINT32_MAX) end[t] = i;
	}
	return end;
}

static void loop_weights (ColorRegs *cr)
{
	uint32_t count = cr->fn->count;
	uint32_t *end = loop_ends(cr);

	for (uint32_t t = 0; t < count; t++) {
		if (end[t] == UINT32_MAX) continue;
		cr->w[t]++;
		if (end[t] + 1 < count) cr->w[end[t] + 1]--;
	}

	int16_t d = 0;
	for (uint32_t i = 0; i < count; i++) {
		d = (int16_t)(d + cr->w[i]);
		int depth = d < LOOP_MAX_DEPTH ? d : LOOP_MAX_DEPTH;
		cr->w[i] = (int16_t)(1 << (LOOP_SHIFT * depth));
	}
}

static void mark_weighted (ColorRegs *cr)
{
	uint32_t count = cr->fn->count ? cr->fn->count : 1;
	cr->w = arena_alloc(cr->arena, count * sizeof(int16_t));
	memset(cr->w, 0, count * sizeof(int16_t));
	loop_weights(cr);
}

static void paint_block (Graph *g)
{
	uint32_t max = g->cr->ctx->scratch_max;

	for (uint32_t s = 0; s <= max; s++) {
		set_palette(g, s);
		reset_nodes(g);
		if (g->node_num && g->k) paint_graph(g);
		if (block_scratch(g) <= s) return;
	}
}

void color_graph (Graph *g)
{
	uint32_t blocks = g->cr->ctx->block_count;

	for (uint32_t i = 0; i < blocks; i++) {
		g->blk = g->order[i];
		build_graph(g);
		paint_block(g);
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

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg)
{
	ColorRegs cr = {
		.arena = arena,
		.ir = ir,
		.fn = fn,
		.regs = regs,
		.ctx = ctx,
		.cfg = cfg,
	};
	color_run(&cr);
}
