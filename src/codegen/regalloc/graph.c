#include "codegen/regalloc/common.h"

#include <string.h>

static uint64_t scratch_mask (uint64_t caller)
{
	uint64_t m = 0;

	for (uint32_t i = 0; i < GRAPH_SCRATCH && caller; i++) {
		uint64_t bit = 1ull << (63 - __builtin_clzll(caller));

		m |= bit;
		caller &= ~bit;
	}
	return m;
}

static void init_pal (Graph *g)
{
	RegCtx *ctx = g->cr->ctx;
	uint64_t all = ctx->caller_mask | ctx->callee_mask;

	g->pal = all & ~scratch_mask(ctx->caller_mask);
	g->pal_callee = g->pal & ctx->callee_mask;
	g->k = (uint32_t)__builtin_popcountll(g->pal);
}

static void alloc_graph (Graph *g)
{
	Arena *a = g->cr->arena;
	uint32_t regs = g->cr->fn->reg_count;
	uint32_t blocks = g->cr->ctx->block_count;
	size_t adj_bytes = (size_t)g->cap * g->stride * sizeof(uint64_t);

	g->nodes = arena_alloc(a, (size_t)g->cap * sizeof(CRNode));
	g->adj = arena_alloc(a, adj_bytes);
	g->live = arena_alloc(a, (size_t)g->stride * sizeof(uint64_t));
	g->vlive = arena_alloc(a, (size_t)((regs + 63) >> 6) * sizeof(uint64_t));
	g->id = arena_alloc(a, (size_t)regs * sizeof(uint32_t));
	g->root = arena_alloc(a, (size_t)regs * sizeof(uint32_t));
	g->stack = arena_alloc(a, (size_t)g->cap * sizeof(uint32_t));
	g->cfg_first = arena_alloc(a, (size_t)(blocks + 1) * sizeof(uint32_t));
	g->order = arena_alloc(a, (size_t)blocks * sizeof(uint32_t));

	memset(g->adj, 0, adj_bytes);
	memset(g->id, 0xFF, (size_t)regs * sizeof(uint32_t));
}

static void init_rows (Graph *g)
{
	uint64_t *row = g->adj;

	for (uint32_t i = 0; i < g->cap; i++) {
		g->nodes[i].row = row;
		row += g->stride;
	}
}

static void init_roots (Graph *g)
{
	Reg *regs = g->cr->regs;

	for (uint32_t v = 0; v < g->cr->fn->reg_count; v++)
		g->root[v] = regs[v].cstate == VR_ALIAS ? regs[v].slot : v;
}

static void init_blocks (Graph *g)
{
	Cfg *cfg = g->cr->cfg;
	RegCtx *ctx = g->cr->ctx;
	uint32_t base = g->cr->fn->start;
	uint32_t c = 0;

	g->cfg_first[0] = 0;
	for (uint32_t b = 0; b < cfg->block_count; b++) {
		uint32_t blk = ctx->ins_block[cfg->blocks[b].start - base];

		while (c < blk) {
			c++;
			g->cfg_first[c] = b;
		}
	}
	g->cfg_first[ctx->block_count] = cfg->block_count;

	for (uint32_t i = 0; i < ctx->block_count; i++)
		g->order[i] = i;
}

void make_graph (ColorRegs *cr, Graph *graph)
{
	uint32_t regs = cr->fn->reg_count;

	memset(graph, 0, sizeof(*graph));
	graph->cr = cr;
	graph->cap = regs < GRAPH_MAX_NODES ? regs : GRAPH_MAX_NODES;
	graph->stride = (graph->cap + 63) >> 6;

	init_pal(graph);
	alloc_graph(graph);
	init_rows(graph);
	init_roots(graph);
	init_blocks(graph);
}

void graph_build (Graph *g)
{
	g->node_num = 0;
	g->stack_num = 0;
}
