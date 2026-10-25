#include "codegen/regalloc/common.h"
#include <stdint.h>

static void push_node (Graph *g, uint32_t i)
{
	CRNode *n = g->nodes + i;

	n->state = CRN_STACKED;
	g->stack[g->stack_num++] = i;
	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = n->row[w];

		while (bits) {
			CRNode *m = g->nodes + (w << 6) + (uint32_t)__builtin_ctzll(bits);

			bits &= bits - 1;
			if (m->state == CRN_ACTIVE) m->deg--;
		}
	}
}

static uint32_t pick_node (Graph *g)
{
	uint32_t best = GRAPH_NO_NODE;

	for (uint32_t i = 0; i < g->node_num; i++) {
		CRNode *n = g->nodes + i;

		if (n->state != CRN_ACTIVE) continue;
		if (n->deg < g->k) return i;
		if (best == GRAPH_NO_NODE) {
			best = i;
			continue;
		}

		CRNode *b = g->nodes + best;
		uint64_t b_score = (n->cost + 1) * b->deg;
		uint64_t n_score = (b->cost + 1) * n->deg;
		if (n_score > b_score) best = i;
	}
	return best;
}

static void neighbor_masks (Graph *g, CRNode *n, uint64_t *used, uint64_t *want)
{
	Reg *regs = g->cr->regs;

	*used = 0;
	*want = 0;
	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = n->row[w];

		while (bits) {
			CRNode *m = g->nodes + (w << 6) + (uint32_t)__builtin_ctzll(bits);
			int h = regs[m->vreg].home[g->blk];

			bits &= bits - 1;
			if (h >= 0) *used |= 1ull << h;
			else *want |= m->pref;
		}
	}
}

static int pick_color (Graph *g, CRNode *n, uint64_t ok, uint64_t want)
{
	uint64_t callee = g->cr->ctx->callee_mask;
	uint64_t cls = n->cross ? ok & callee : ok & ~callee;

	if (!cls) {
		if (n->cross && n->cross * 2 >= n->cost) return -1;
		cls = ok;
	}
	if (cls & n->pref) cls &= n->pref;
	else if (cls & ~want) cls &= ~want;
	return 63 - __builtin_clzll(cls);
}

static void select_colors (Graph *g)
{
	Reg *regs = g->cr->regs;

	while (g->stack_num) {
		CRNode *n = g->nodes + g->stack[--g->stack_num];
		uint64_t used, want;
		int h = -1;

		neighbor_masks(g, n, &used, &want);
		if (n->avail & ~used) h = pick_color(g, n, n->avail & ~used, want);
		if (h >= 0) regs[n->vreg].home[g->blk] = (int8_t)h;
	}
}

void paint_graph (Graph *g)
{
	while (g->stack_num < g->node_num)
		push_node(g, pick_node(g));
	select_colors(g);
}
