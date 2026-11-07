#include "ir/opts/common.h"

#include <string.h>

static const uint8_t stops_static[IR_COUNT] = {
	[IR_JMP] = 1, [IR_JZ] = 1, [IR_JNZ] = 1, [IR_LABEL] = 1, [IR_RET] = 1
};

static int bind_value (Optimizer *opt, IRInstr *cst, IRGlobal *g)
{
	uint8_t size = types[g->type].size;

	if (cst->op == IR_STR_LIT || cst->op == IR_ADDR_GLOBAL) {
		uint32_t limit = cst->op == IR_STR_LIT
				? opt->ir->string_count : opt->ir->global_count;

		if (size != 8 || types[cst->data_type].size != 8 || cst->target >= limit) return 0;
		g->init_kind = cst->op == IR_STR_LIT ? IR_INIT_STR : IR_INIT_ADDR;
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

static void touch_direct (IR *ir, IRFn *fn)
{
	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = ir->instrs + fn->start + j;

		if (in->op != IR_LD_GLOBAL && in->op != IR_STR_GLOBAL) continue;
		fn->gtouch[in->target >> 6] |= 1ull << (in->target & 63);
	}
}

static int touch_merge (IR *ir, IRFn *fn, uint32_t words)
{
	int changed = 0;

	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = ir->instrs + fn->start + j;
		const uint64_t *src;

		if (in->op != IR_CALL) continue;
		src = ir->fns[in->target].gtouch;
		for (uint32_t w = 0; w < words; w++) {
			changed |= (src[w] & ~fn->gtouch[w]) != 0;
			fn->gtouch[w] |= src[w];
		}
	}
	return changed;
}

static void compute_touch (Optimizer *opt)
{
	IR *ir = opt->ir;
	uint32_t words = (ir->global_count + 63) >> 6;
	int changed = 1;

	touch_alloc(opt, words);
	for (uint32_t i = 0; i < ir->fn_count; i++)
		touch_direct(ir, ir->fns + i);
	while (changed) {
		changed = 0;
		for (uint32_t i = 0; i < ir->fn_count; i++)
			changed |= touch_merge(ir, ir->fns + i, words);
	}
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
