#include "ir/opts/common.h"

#include <string.h>

static const uint8_t stops_static[IR_COUNT] = {
	[IR_JMP] = 1, [IR_JZ] = 1, [IR_JNZ] = 1, [IR_JMP_TBL] = 1,
	[IR_LABEL] = 1, [IR_RET] = 1, [IR_CALL_IND] = 1
};

static int bind_value (Optimizer *opt, IRInstr *cst, IRGlobal *g)
{
	uint8_t size = types[g->type].size;

	if (cst->op == IR_STR_LIT || cst->op == IR_ADDR_GLOBAL
			|| cst->op == IR_FN_ADDR) {
		uint32_t limit = cst->op == IR_ADDR_GLOBAL
				? opt->ir->global_count : opt->ir->string_count;

		if (cst->op == IR_FN_ADDR) limit = opt->ir->fn_count;
		if (size != 8 || types[cst->data_type].size != 8 || cst->target >= limit) return 0;
		if (cst->op == IR_STR_LIT) g->init_kind = IR_INIT_STR;
		else if (cst->op == IR_FN_ADDR) g->init_kind = IR_INIT_FN;
		else g->init_kind = IR_INIT_ADDR;
		g->init_ref = cst->target;
		return 1;
	}
	if (cst->op != IR_CONST) return 0;
	if (types[cst->data_type].size != size) return 0;
	if (size < 8 && cst->data_type != g->type) return 0;

	g->init = cst->imm64;
	return 1;
}

static int bind_static (Optimizer *opt, OptFn *f, IRInstr *st, const uint8_t *addr_taken)
{
	IRInstr *first = opt->ir->instrs + f->fn->start;
	IRGlobal *g = opt->ir->globals + st->target;
	IRInstr *cst;

	if (type_kind(g->type) == TK_ARRAY || type_kind(g->type) == TK_SLICE)
		return 0;
	if (addr_taken[st->target]) return 0;
	if (st == first) return 0;
	cst = st - 1;
	while (cst > first && cst->op == IR_NOP) cst--;
	for (IRInstr *p = cst + 1; p < st; p++)
		if (stops_static[p->op]) return 0;
	if (cst->dst != st->src1) return 0;
	if (!bind_value(opt, cst, g)) return 0;

	g->has_init = 1;
	if (f->uses[cst->dst] == 1) kill_instr(cst);
	kill_instr(st);
	return 1;
}

static void touch_alloc (Optimizer *opt, uint32_t words)
{
	IR *ir = opt->ir;
	size_t bytes = (size_t)ir->fn_count * words * sizeof(uint64_t);
	uint64_t *buf = arena_alloc(opt->arena, bytes);

	memset(buf, 0, bytes);
	for (uint32_t i = 0; i < ir->fn_count; i++, buf += words)
		ir->fns[i].gtouch = buf;
}

static void touch_direct (IR *ir, IRFn *fn, uint32_t words)
{
	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = ir->instrs + fn->start + j;

		if (in->op == IR_CALL_IND) {
			for (uint32_t w = 0; w < words; w++)
				fn->gtouch[w] = ~0ull;
			continue;
		}
		if (in->op != IR_LD_GLOBAL && in->op != IR_STR_GLOBAL) continue;
		fn->gtouch[in->target >> 6] |= 1ull << (in->target & 63);
	}
}

typedef struct TouchGraph {
	uint32_t *start;
	uint32_t *list;
} TouchGraph;

static void touch_graph (IR *ir, Arena *a, TouchGraph *g)
{
	uint32_t n = ir->fn_count;
	uint32_t *cnt = arena_alloc(a, (size_t)(n + 1) * sizeof(uint32_t));
	uint32_t *cur;

	memset(cnt, 0, (size_t)(n + 1) * sizeof(uint32_t));
	for (uint32_t i = 0; i < n; i++) {
		IRFn *fn = ir->fns + i;
		for (uint32_t j = 0; j < fn->count; j++) {
			IRInstr *in = ir->instrs + fn->start + j;

			if (in->op == IR_CALL) cnt[in->target]++;
		}
	}
	g->start = arena_alloc(a, (size_t)(n + 1) * sizeof(uint32_t));
	g->start[0] = 0;
	for (uint32_t i = 0; i < n; i++) g->start[i + 1] = g->start[i] + cnt[i];
	g->list = arena_alloc(a, (g->start[n] ? g->start[n] : 1) * sizeof(uint32_t));
	cur = arena_alloc(a, (size_t)n * sizeof(uint32_t));
	memcpy(cur, g->start, (size_t)n * sizeof(uint32_t));
	for (uint32_t i = 0; i < n; i++) {
		IRFn *fn = ir->fns + i;
		for (uint32_t j = 0; j < fn->count; j++) {
			IRInstr *in = ir->instrs + fn->start + j;

			if (in->op == IR_CALL) g->list[cur[in->target]++] = i;
		}
	}
}

static void touch_propagate (IR *ir, const TouchGraph *g, uint32_t words, uint32_t *work, uint32_t *queued)
{
	uint32_t sp = 0;

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		work[sp++] = i;
		queued[i] = 1;
	}
	while (sp) {
		uint32_t f = work[--sp];
		const uint64_t *src = ir->fns[f].gtouch;

		queued[f] = 0;
		for (uint32_t k = g->start[f]; k < g->start[f + 1]; k++) {
			uint32_t c = g->list[k];
			uint64_t *dst = ir->fns[c].gtouch;
			int changed = 0;

			for (uint32_t w = 0; w < words; w++) {
				changed |= (src[w] & ~dst[w]) != 0;
				dst[w] |= src[w];
			}
			if (changed && !queued[c]) {
				queued[c] = 1;
				work[sp++] = c;
			}
		}
	}
}

static void compute_touch (Optimizer *opt)
{
	IR *ir = opt->ir;
	uint32_t words = (ir->global_count + 63) >> 6;

	if (!words) return;
	touch_alloc(opt, words);
	for (uint32_t i = 0; i < ir->fn_count; i++)
		touch_direct(ir, ir->fns + i, words);

	ArenaMark mark = arena_mark(opt->arena);
	TouchGraph g;
	uint32_t *work = arena_alloc(opt->arena, (size_t)ir->fn_count * sizeof(uint32_t));
	uint32_t *queued = arena_alloc(opt->arena, (size_t)ir->fn_count * sizeof(uint32_t));

	memset(queued, 0, (size_t)ir->fn_count * sizeof(uint32_t));
	touch_graph(ir, opt->arena, &g);
	touch_propagate(ir, &g, words, work, queued);
	arena_release(opt->arena, &mark);
}

static void call_dirty (IR *ir, IRInstr *call, uint64_t *dirty, uint32_t words)
{
	const uint64_t *touched = ir->fns[call->target].gtouch;

	for (uint32_t w = 0; w < words; w++)
		dirty[w] |= touched[w];
}

static uint8_t *addr_taken_map (Optimizer *opt)
{
	IR *ir = opt->ir;
	uint8_t *taken = arena_alloc(opt->arena, ir->global_count + 1);

	memset(taken, 0, ir->global_count + 1);
	for (uint32_t i = 0; i < ir->instr_count; i++) {
		IRInstr *in = ir->instrs + i;

		if (in->op == IR_ADDR_GLOBAL && in->target < ir->global_count)
			taken[in->target] = 1;
	}
	return taken;
}

static int scan_static_init (Optimizer *opt, OptFn *f, const uint8_t *addr_taken)
{
	IR *ir = opt->ir;
	uint32_t words = (ir->global_count + 63) >> 6;
	uint8_t *seen = arena_alloc(opt->arena, ir->global_count + 1);
	uint64_t *dirty = arena_alloc(opt->arena, (words + 1) * sizeof(uint64_t));
	int changed = 0;

	memset(seen, 0, ir->global_count + 1);
	memset(dirty, 0, (words + 1) * sizeof(uint64_t));
	for (uint32_t i = 0; i < f->fn->count; i++) {
		IRInstr *in = ir->instrs + f->fn->start + i;

		if (stops_static[in->op]) break;
		if (in->op == IR_CALL) call_dirty(ir, in, dirty, words);
		if (in->op != IR_STR_GLOBAL && in->op != IR_LD_GLOBAL) continue;
		if (seen[in->target]) continue;
		seen[in->target] = 1;
		if (in->op == IR_STR_GLOBAL && !(dirty[in->target >> 6] >> (in->target & 63) & 1))
			changed |= bind_static(opt, f, in, addr_taken);
	}
	return changed;
}

void run_static_init (Optimizer *opt)
{
	if (!opt->ir->global_count) return;
	uint8_t *addr_taken = addr_taken_map(opt);
	compute_touch(opt);

	OptFn *f = &opt->cur;
	f->fn = opt->ir->fns + opt->ir->init_fn;
	analyze_fn(opt, f);
	if (scan_static_init(opt, f, addr_taken)) optimize_fn(opt, f);
}
