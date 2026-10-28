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
		if (n->deg < (uint32_t)__builtin_popcountll(n->avail)) return i;
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
	uint64_t pick = cls & n->hint & n->pref;
	if (!pick) pick = cls & n->hint;
	if (!pick) pick = cls & n->pref;

	if (pick) cls = pick;
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

static void copy_merged (Graph *g)
{
	Reg *regs = g->cr->regs;

	for (uint32_t i = 0; i < g->node_num; i++) {
		CRNode *n = g->nodes + i;
		uint32_t r;

		if (n->state != CRN_MERGED) continue;
		r = g->nodes[node_find(g, i)].vreg;
		regs[n->vreg].home[g->blk] = regs[r].home[g->blk];
	}
}

void paint_graph (Graph *g)
{
	while (g->stack_num + g->merged < g->node_num)
		push_node(g, pick_node(g));
	select_colors(g);
	copy_merged(g);
}

uint32_t node_find (Graph *g, uint32_t id)
{
	while (g->nodes[id].rep != id) {
		g->nodes[id].rep = g->nodes[g->nodes[id].rep].rep;
		id = g->nodes[id].rep;
	}
	return id;
}

static uint32_t node_degree (Graph *g, uint32_t id)
{
	uint32_t deg = 0;

	for (uint32_t w = 0; w < g->stride; w++)
		deg += (uint32_t)__builtin_popcountll(g->nodes[id].row[w]);
	return deg;
}

static int briggs_ok (Graph *g, uint32_t a, uint32_t b)
{
	CRNode *na = g->nodes + a;
	CRNode *nb = g->nodes + b;
	uint32_t k = (uint32_t)__builtin_popcountll(g->bpal & ~(na->forbid | nb->forbid));
	uint32_t big = 0;

	if (!k) return 0;
	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = na->row[w] | nb->row[w];

		while (bits) {
			uint32_t t = (w << 6) + (uint32_t)__builtin_ctzll(bits);

			bits &= bits - 1;
			if (node_degree(g, t) >= k) big++;
		}
	}
	return big < k;
}

static void merge_nodes (Graph *g, uint32_t a, uint32_t b)
{
	CRNode *na = g->nodes + a;
	CRNode *nb = g->nodes + b;

	for (uint32_t w = 0; w < g->stride; w++) {
		uint64_t bits = nb->row[w];

		while (bits) {
			uint32_t t = (w << 6) + (uint32_t)__builtin_ctzll(bits);

			bits &= bits - 1;
			g->nodes[t].row[b >> 6] &= ~(1ull << (b & 63));
			g->nodes[t].row[a >> 6] |= 1ull << (a & 63);
			na->row[t >> 6] |= 1ull << (t & 63);
		}
		nb->row[w] = 0;
	}
	na->cost += nb->cost;
	na->cross += nb->cross;
	na->pref |= nb->pref;
	na->hint |= nb->hint;
	na->forbid |= nb->forbid;
	nb->rep = a;
	nb->state = CRN_MERGED;
	g->merged++;
}

static void sort_affinities (Graph *g)
{
	for (uint32_t i = 1; i < g->aff_num; i++) {
		Affinity key = g->aff[i];
		uint32_t j = i;

		while (j > 0 && g->aff[j - 1].wt < key.wt) {
			g->aff[j] = g->aff[j - 1];
			j--;
		}
		g->aff[j] = key;
	}
}

void coalesce_graph (Graph *g)
{
	sort_affinities(g);
	for (uint32_t i = 0; i < g->aff_num; i++) {
		uint32_t a = node_find(g, g->aff[i].a);
		uint32_t b = node_find(g, g->aff[i].b);

		if (a == b) continue;
		if (g->nodes[a].row[b >> 6] >> (b & 63) & 1) continue;
		if (!briggs_ok(g, a, b)) continue;
		merge_nodes(g, a, b);
	}
}
