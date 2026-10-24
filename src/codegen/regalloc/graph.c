#include "codegen/regalloc/common.h"

#include <string.h>

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

void make_graph (ColorRegs *cr, Graph *g)
{
	uint32_t regs = cr->fn->reg_count;

	memset(g, 0, sizeof(Graph));
	g->cr = cr;
	g->cap = regs < GRAPH_MAX_NODES ? regs : GRAPH_MAX_NODES;
	g->stride = (g->cap + 63) >> 6;
	g->pal = cr->ctx->caller_mask | cr->ctx->callee_mask;

	alloc_graph(g);
	init_rows(g);
	init_roots(g);
	init_blocks(g);
}

static uint64_t block_reserved (Graph *g)
{
	ColorRegs *cr = g->cr;
	uint64_t m = 0;

	for (uint32_t b = g->cfg_first[g->blk]; b < g->cfg_first[g->blk + 1]; b++) {
		CfgBlock *blk = cr->cfg->blocks + b;
		IRInstr *code = cr->ir->instrs + blk->start;

		for (uint32_t i = 0; i < blk->count; i++) {
			OpNeeds *n = cr->ctx->needs + code[i].op;
			m |= n->destroy_mask;
			if (n->src1.type == REG_COND_FIXED) m |= 1ull << n->src1.reg;
			if (n->src2.type == REG_COND_FIXED) m |= 1ull << n->src2.reg;
			if (n->dst.type == REG_COND_FIXED) m |= 1ull << n->dst.reg;
		}
	}
	return m;
}

void build_graph (Graph *g)
{
	g->node_num = 0;
	g->stack_num = 0;
	g->bpal = g->pal & ~block_reserved(g);
	g->k = (uint32_t)__builtin_popcountll(g->bpal);
}
