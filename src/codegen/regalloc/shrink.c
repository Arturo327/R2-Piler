#include "codegen/regalloc/common.h"

#include <string.h>

#define SHRINK_MAX_BLOCKS 2048u

typedef struct Shrink {
	RegAlloc *ra;
	Arena *arena;

	uint32_t blocks;
	uint32_t words;
	uint32_t epoch;

	uint64_t *dom;
	uint64_t *ndom;
	uint64_t *touch;
	uint64_t *saved;

	uint32_t *first;
	uint32_t *adj;
	uint32_t *stack;
	uint32_t *stamp;

	uint8_t *reach;
	uint8_t *is_exit;
} Shrink;

static void touch_live (Shrink *s, uint32_t b, uint32_t p, const uint64_t *live)
{
	for (uint32_t w = 0; w < s->ra->words; w++) {
		uint64_t bits = live[w];

		while (bits) {
			uint32_t v = (w << 6) + (uint32_t)__builtin_ctzll(bits);
			int h = s->ra->regs[v].home[p];

			bits &= bits - 1;
			if (h >= 0 && (s->ra->ctx->arch->callee_mask >> h & 1))
				s->touch[b] |= 1ull << h;
		}
	}
}

static void touch_instrs (Shrink *s)
{
	RegAlloc *ra = s->ra;

	for (uint32_t b = 0; b < s->blocks; b++) {
		CfgBlock *blk = ra->cfg.blocks + b;
		uint32_t p = ra->ctx->ins_block[blk->start - ra->fn->start];

		if (!s->reach[b]) continue;
		for (uint32_t i = 0; i < blk->count; i++) {
			IRInstr *in = ra->ir->instrs + blk->start + i;
			uint32_t *src[3];
			int n = get_srcs(in, src);

			if (in->dst != NO_REG) {
				int h = ra->regs[in->dst].home[p];

				if (h >= 0 && (ra->ctx->arch->callee_mask >> h & 1))
					s->touch[b] |= 1ull << h;
			}
			for (int k = 0; k < n; k++) {
				int h = ra->regs[*src[k]].home[p];

				if (h >= 0 && (ra->ctx->arch->callee_mask >> h & 1))
					s->touch[b] |= 1ull << h;
			}
		}
		touch_live(s, b, p, blk->live_in);
		cfg_live_out(&ra->cfg, b, ra->tmp, ra->words);
		touch_live(s, b, p, ra->tmp);
	}
}

static void touch_edge_live (Shrink *s, uint32_t b, uint32_t ps, uint32_t pt,
		const uint64_t *live)
{
	for (uint32_t w = 0; w < s->ra->words; w++) {
		uint64_t bits = live[w];

		while (bits) {
			uint32_t v = (w << 6) + (uint32_t)__builtin_ctzll(bits);
			int hs = s->ra->regs[v].home[ps];
			int ht = s->ra->regs[v].home[pt];

			bits &= bits - 1;
			if (hs != VR_NO_HOME && ht != hs && ht >= 0 &&
					(s->ra->ctx->arch->callee_mask >> ht & 1))
				s->touch[b] |= 1ull << ht;
		}
	}
}

static void touch_edges (Shrink *s)
{
	RegAlloc *ra = s->ra;

	for (uint32_t b = 0; b < s->blocks; b++) {
		CfgBlock *blk = ra->cfg.blocks + b;
		uint32_t ps = ra->ctx->ins_block[blk->start - ra->fn->start];

		if (!s->reach[b]) continue;
		for (uint32_t k = 0; k < blk->succ_count; k++) {
			CfgBlock *dst = ra->cfg.blocks + blk->succ[k];
			uint32_t pt = ra->ctx->ins_block[dst->start - ra->fn->start];

			touch_edge_live(s, b, ps, pt, dst->live_in);
		}
	}
}

static void mark_exits (Shrink *s)
{
	RegAlloc *ra = s->ra;

	for (uint32_t b = 0; b < s->blocks; b++) {
		CfgBlock *blk = ra->cfg.blocks + b;

		if (!s->reach[b]) continue;
		for (uint32_t i = 0; i < blk->count; i++) {
			IRInstr *in = ra->ir->instrs + blk->start + i;
			int tail = in->op == IR_CALL && cg_is_tail_call(ra->ir, ra->fn,
					ra->regs, ra->ctx, in, ra->ctx->arch->arg_reg_max);

			if (in->op == IR_RET || tail) {
				s->is_exit[b] = 1;
				break;
			}
		}
	}
}

static void build_adj (Shrink *s)
{
	RegAlloc *ra = s->ra;
	uint32_t n = 0;

	for (uint32_t b = 0; b < s->blocks; b++)
		n += ra->cfg.blocks[b].succ_count;
	s->first = arena_alloc(s->arena, (size_t)(s->blocks + 1) * sizeof(uint32_t));
	s->adj = arena_alloc(s->arena, (size_t)(n ? n : 1) * sizeof(uint32_t));
	s->first[0] = 0;
	for (uint32_t b = 0; b < s->blocks; b++) {
		CfgBlock *blk = ra->cfg.blocks + b;

		for (uint32_t k = 0; k < blk->succ_count; k++)
			s->adj[s->first[b] + k] = blk->succ[k];
		s->first[b + 1] = s->first[b] + blk->succ_count;
	}
}

static void mark_reach (Shrink *s)
{
	uint32_t sp = 0;

	memset(s->reach, 0, s->blocks);
	s->reach[0] = 1;
	s->stack[sp++] = 0;
	while (sp) {
		uint32_t u = s->stack[--sp];

		for (uint32_t e = s->first[u]; e < s->first[u + 1]; e++) {
			uint32_t v = s->adj[e];

			if (s->reach[v]) continue;
			s->reach[v] = 1;
			s->stack[sp++] = v;
		}
	}
}

static uint32_t block_depth (Shrink *s, uint32_t b)
{
	const uint64_t *row = s->dom + (size_t)b * s->words;
	uint32_t depth = 0;

	for (uint32_t w = 0; w < s->words; w++)
		depth += (uint32_t)__builtin_popcountll(row[w]);
	return depth;
}

static void dom_init (Shrink *s)
{
	uint64_t full = s->blocks >= 64 ? ~0ull : (1ull << s->blocks) - 1;

	for (uint32_t b = 0; b < s->blocks; b++) {
		uint64_t *row = s->dom + (size_t)b * s->words;

		for (uint32_t w = 0; w < s->words; w++)
			row[w] = s->reach[b] ? full : 0;
	}
	for (uint32_t w = 0; w < s->words; w++)
		s->dom[w] = 0;
	s->dom[0] = 1;
}

static int dom_step (Shrink *s)
{
	uint64_t full = s->blocks >= 64 ? ~0ull : (1ull << s->blocks) - 1;
	int changed = 0;

	for (uint32_t b = 0; b < s->blocks; b++) {
		uint64_t *row = s->ndom + (size_t)b * s->words;

		for (uint32_t w = 0; w < s->words; w++)
			row[w] = s->reach[b] ? full : 0;
	}
	for (uint32_t p = 0; p < s->blocks; p++) {
		const uint64_t *src;

		if (!s->reach[p]) continue;
		src = s->dom + (size_t)p * s->words;
		for (uint32_t e = s->first[p]; e < s->first[p + 1]; e++) {
			uint64_t *dst = s->ndom + (size_t)s->adj[e] * s->words;

			for (uint32_t w = 0; w < s->words; w++)
				dst[w] &= src[w];
		}
	}
	for (uint32_t b = 0; b < s->blocks; b++) {
		if (!s->reach[b]) continue;
		s->ndom[(size_t)b * s->words + (b >> 6)] |= 1ull << (b & 63);
	}
	for (uint32_t w = 0; w < s->words; w++)
		s->ndom[w] = 0;
	s->ndom[0] = 1;
	for (uint32_t b = 0; b < s->blocks; b++) {
		const uint64_t *a = s->dom + (size_t)b * s->words;
		const uint64_t *c = s->ndom + (size_t)b * s->words;

		for (uint32_t w = 0; w < s->words; w++)
			if (a[w] != c[w]) changed = 1;
	}
	uint64_t *t = s->dom;
	s->dom = s->ndom;
	s->ndom = t;
	return changed;
}

static int reaches (Shrink *s, uint32_t from, uint32_t to)
{
	uint32_t sp = 0;

	s->epoch++;
	s->stack[sp++] = from;
	s->stamp[from] = s->epoch;
	while (sp) {
		uint32_t u = s->stack[--sp];

		if (u == to) return 1;
		for (uint32_t e = s->first[u]; e < s->first[u + 1]; e++) {
			uint32_t v = s->adj[e];

			if (s->stamp[v] == s->epoch) continue;
			s->stamp[v] = s->epoch;
			s->stack[sp++] = v;
		}
	}
	return 0;
}

static int exits_ok (Shrink *s, uint32_t b)
{
	for (uint32_t e = 0; e < s->blocks; e++) {
		const uint64_t *row;

		if (!s->is_exit[e] || !reaches(s, b, e)) continue;
		row = s->dom + (size_t)e * s->words;
		if (!(row[b >> 6] >> (b & 63) & 1)) return 0;
	}
	return 1;
}

static uint32_t save_at (Shrink *s, uint32_t b)
{
	IR *ir = s->ra->ir;
	IRFn *fn = s->ra->fn;
	CfgBlock *blk = s->ra->cfg.blocks + b;

	if (b == 0) return 0;
	for (uint32_t i = 0; i < blk->count; i++)
		if (ir->instrs[blk->start + i].op != IR_LABEL)
			return blk->start + i - fn->start;
	return UINT32_MAX;
}

static uint32_t pick_save_block (Shrink *s, const uint64_t *common)
{
	uint32_t best = 0;
	uint32_t depth = block_depth(s, 0);

	for (uint32_t b = 1; b < s->blocks; b++) {
		uint32_t d;
		int cyclic = 0;

		if (!s->reach[b] || !(common[b >> 6] >> (b & 63) & 1)) continue;
		if (save_at(s, b) == UINT32_MAX) continue;
		for (uint32_t e = s->first[b]; e < s->first[b + 1]; e++)
			if (reaches(s, s->adj[e], b)) { cyclic = 1; break; }
		if (cyclic || !exits_ok(s, b)) continue;
		d = block_depth(s, b);
		if (d > depth) {
			best = b;
			depth = d;
		}
	}
	return best;
}

static void plan_reg (Shrink *s, uint32_t r)
{
	uint64_t bit = 1ull << r;
	uint64_t full = s->blocks >= 64 ? ~0ull : (1ull << s->blocks) - 1;
	uint64_t *common = s->ndom;
	uint32_t touched = 0;

	for (uint32_t w = 0; w < s->words; w++)
		common[w] = full;
	for (uint32_t b = 0; b < s->blocks; b++) {
		const uint64_t *row = s->dom + (size_t)b * s->words;

		if (!s->reach[b] || !(s->touch[b] & bit)) continue;
		for (uint32_t w = 0; w < s->words; w++)
			common[w] &= row[w];
		touched = 1;
	}
	if (!touched) return;

	uint32_t b = pick_save_block(s, common);
	uint32_t at = save_at(s, b);

	s->ra->ctx->save_in[at] |= bit;
	for (uint32_t q = 0; q < s->blocks; q++) {
		const uint64_t *row = s->dom + (size_t)q * s->words;

		if (s->reach[q] && (row[b >> 6] >> (b & 63) & 1))
			s->saved[q] |= bit;
	}
}

static void mark_restore (Shrink *s)
{
	RegAlloc *ra = s->ra;
	IRFn *fn = ra->fn;

	for (uint32_t b = 0; b < s->blocks; b++) {
		CfgBlock *blk = ra->cfg.blocks + b;

		if (!s->reach[b] || !s->saved[b]) continue;
		for (uint32_t i = 0; i < blk->count; i++) {
			IRInstr *in = ra->ir->instrs + blk->start + i;
			int tail = in->op == IR_CALL && cg_is_tail_call(ra->ir, ra->fn,
					ra->regs, ra->ctx, in, ra->ctx->arch->arg_reg_max);

			if (in->op == IR_RET || tail)
				ra->ctx->restore_at[blk->start + i - fn->start] |= s->saved[b];
		}
	}
}

static int has_self_tail (RegAlloc *ra)
{
	uint32_t self = (uint32_t)(ra->fn - ra->ir->fns);

	for (uint32_t i = 0; i < ra->fn->count; i++) {
		IRInstr *in = ra->ir->instrs + ra->fn->start + i;

		if (in->op == IR_CALL && in->target == self &&
				cg_is_tail_call(ra->ir, ra->fn, ra->regs, ra->ctx, in,
						ra->ctx->arch->arg_reg_max))
			return 1;
	}
	return 0;
}

static void save_all (RegAlloc *ra)
{
	uint64_t mask = ra->ctx->callee_used;

	ra->ctx->save_in[0] = mask;
	for (uint32_t i = 0; i < ra->fn->count; i++) {
		IRInstr *in = ra->ir->instrs + ra->fn->start + i;
		int tail = in->op == IR_CALL && cg_is_tail_call(ra->ir, ra->fn,
				ra->regs, ra->ctx, in, ra->ctx->arch->arg_reg_max);

		if (in->op == IR_RET || tail)
			ra->ctx->restore_at[i] = mask;
	}
}

void shrink_wrap (RegAlloc *ra)
{
	Shrink s = { .ra = ra, .arena = ra->arena, .blocks = ra->cfg.block_count };

	if (!ra->ctx->callee_used || !ra->words) return;
	if (s.blocks == 0 || s.blocks > SHRINK_MAX_BLOCKS || has_self_tail(ra)) {
		save_all(ra);
		return;
	}
	s.words = (s.blocks + 63) >> 6;
	s.dom = arena_alloc(s.arena, (size_t)s.blocks * s.words * sizeof(uint64_t));
	s.ndom = arena_alloc(s.arena, (size_t)s.blocks * s.words * sizeof(uint64_t));
	s.touch = arena_alloc(s.arena, (size_t)s.blocks * sizeof(uint64_t));
	s.saved = arena_alloc(s.arena, (size_t)s.blocks * sizeof(uint64_t));
	s.stack = arena_alloc(s.arena, (size_t)s.blocks * sizeof(uint32_t));
	s.stamp = arena_alloc(s.arena, (size_t)s.blocks * sizeof(uint32_t));
	s.reach = arena_alloc(s.arena, s.blocks);
	s.is_exit = arena_alloc(s.arena, s.blocks);
	memset(s.touch, 0, (size_t)s.blocks * sizeof(uint64_t));
	memset(s.saved, 0, (size_t)s.blocks * sizeof(uint64_t));
	memset(s.stamp, 0, (size_t)s.blocks * sizeof(uint32_t));
	memset(s.is_exit, 0, s.blocks);

	build_adj(&s);
	mark_reach(&s);
	mark_exits(&s);
	dom_init(&s);
	while (dom_step(&s))
		;
	touch_instrs(&s);
	touch_edges(&s);
	for (uint32_t r = 0; r < 64; r++)
		if (ra->ctx->arch->callee_mask >> r & 1)
			plan_reg(&s, r);
	mark_restore(&s);
}