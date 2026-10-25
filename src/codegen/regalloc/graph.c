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

static uint32_t add_node (Graph *g, uint32_t v)
{
	uint32_t r = g->root[v];
	Reg *reg = g->cr->regs + r;

	if (g->id[r] != GRAPH_NO_NODE) return g->id[r];
	if (reg->cstate != VR_MEM || !reg->uses) return GRAPH_NO_NODE;
	if (g->node_num >= g->cap) return GRAPH_NO_NODE;

	CRNode *n = g->nodes + g->node_num;
	n->vreg = r;
	n->avail = g->bpal;
	n->pref = 0;
	n->cost = 0;
	n->cross = 0;
	n->deg = 0;
	n->state = CRN_ACTIVE;
	g->id[r] = g->node_num++;

	return g->id[r];
}

static void reset_graph (Graph *g)
{
	for (uint32_t i = 0; i < g->node_num; i++)
		g->id[g->nodes[i].vreg] = GRAPH_NO_NODE;

	size_t bytes = (size_t)g->node_num * g->stride * sizeof(uint64_t);
	memset(g->adj, 0, bytes);

	g->node_num = 0;
	g->stack_num = 0;
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

static uint32_t live_add (Graph *g, uint32_t v)
{
	uint32_t id = add_node(g, v);

	if (id != GRAPH_NO_NODE)
		g->live[id >> 6] |= 1ull << (id & 63);
	return id;
}

static void load_live (Graph *g, uint32_t b)
{
	uint32_t words = (g->cr->fn->reg_count + 63) >> 6;

	memset(g->live, 0, (size_t)g->stride * sizeof(uint64_t));
	cfg_live_out(g->cr->cfg, b, g->vlive, words);
	for (uint32_t w = 0; w < words; w++) {
		uint64_t bits = g->vlive[w];

		while (bits) {
			live_add(g, (w << 6) + (uint32_t)__builtin_ctzll(bits));
			bits &= bits - 1;
		}
	}
}

static void link_pair (Graph *g, uint32_t a, uint32_t b)
{
	g->nodes[a].row[b >> 6] |= 1ull << (b & 63);
	g->nodes[b].row[a >> 6] |= 1ull << (a & 63);
}

static void link_live (Graph *g, uint32_t a)
{
	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = g->live[w];

		while (bits) {
			uint32_t b = (w << 6) + (uint32_t)__builtin_ctzll(bits);
			bits &= bits - 1;
			if (b != a) link_pair(g, a, b);
		}
	}
}

static void clique_live (Graph *g)
{
	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = g->live[w];

		while (bits) {
			uint32_t a = (w << 6) + (uint32_t)__builtin_ctzll(bits);
			bits &= bits - 1;
			link_live(g, a);
		}
	}
}

static void cross_live (Graph *g, uint32_t wt)
{
	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = g->live[w];

		while (bits) {
			uint32_t id = (w << 6) + (uint32_t)__builtin_ctzll(bits);
			bits &= bits - 1;
			g->nodes[id].cross += wt;
		}
	}
}

static void sweep_instr (Graph *g, IRInstr *in, uint32_t at)
{
	uint32_t wt = (uint32_t)g->cr->w[at];
	uint32_t *src[3];
	int n = get_srcs(in, src);
	uint32_t d = GRAPH_NO_NODE;

	if (in->dst != NO_REG) d = add_node(g, in->dst);
	if (d != GRAPH_NO_NODE) {
		link_live(g, d);
		g->nodes[d].cost += wt;
		g->live[d >> 6] &= ~(1ull << (d & 63));
	}

	if (in->op == IR_CALL) cross_live(g, wt);
	for (int k = 0; k < n; k++) {
		uint32_t s = live_add(g, *src[k]);
		if (s == GRAPH_NO_NODE) continue;
		g->nodes[s].cost += wt;
	}
}

static void sweep_block (Graph *g, uint32_t b)
{
	CfgBlock *blk = g->cr->cfg->blocks + b;
	IRInstr *code = g->cr->ir->instrs + blk->start;
	uint32_t base = blk->start - g->cr->fn->start;

	load_live(g, b);
	for (uint32_t i = blk->count; i-- > 0;) {
		if (code[i].op == IR_NOP) continue;
		sweep_instr(g, code + i, base + i);
	}
	clique_live(g);
}

static void finish_nodes (Graph *g)
{
	Reg *regs = g->cr->regs;

	for (uint32_t i = 0; i < g->node_num; i++) {
		CRNode *n = g->nodes + i;
		int h = g->blk ? regs[n->vreg].home[g->blk - 1] : VR_SPILL;

		n->deg = 0;
		for (uint32_t w = 0; w < g->stride; w++)
			n->deg += (uint32_t)__builtin_popcountll(n->row[w]);
		n->pref = h >= 0 ? 1ull << h : 0;
	}
}

void build_graph (Graph *g)
{
	reset_graph(g);
	g->bpal = g->pal & ~block_reserved(g);
	g->k = (uint32_t)__builtin_popcountll(g->bpal);
	if (!g->k) return;

	for (uint32_t b = g->cfg_first[g->blk]; b < g->cfg_first[g->blk + 1]; b++)
		sweep_block(g, b);
	finish_nodes(g);
}
