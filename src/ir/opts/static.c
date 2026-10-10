#include "ir/opts/common.h"

#include <string.h>

static const uint8_t stops_static[IR_COUNT] = {
	[IR_JMP] = 1, [IR_JZ] = 1, [IR_JNZ] = 1, [IR_LABEL] = 1
};

static int bind_static (Optimizer *opt, OptFn *f, IRInstr *st)
{
	IRInstr *first = opt->ir->instrs + f->fn->start;
	IRGlobal *g = opt->ir->globals + st->target;
	IRInstr *cst;

	if (st == first) return 0;
	cst = st - 1;
	while (cst > first && cst->op == IR_NOP) cst--;
	if (cst->op != IR_CONST || cst->dst != st->src1) return 0;

	if (types[cst->data_type].size != types[g->type].size) return 0;
	if (types[g->type].size < 8 && cst->data_type != g->type) return 0;

	g->has_init = 1;
	g->init = cst->imm64;
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

static int scan_static_init (Optimizer *opt, OptFn *f)
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
			changed |= bind_static(opt, f, in);
	}
	return changed;
}

void run_static_init (Optimizer *opt)
{
	OptFn *f = &opt->cur;

	if (!opt->ir->global_count) return;
	compute_touch(opt);
	f->fn = opt->ir->fns + opt->ir->init_fn;
	analyze_fn(opt, f);
	if (scan_static_init(opt, f)) optimize_fn(opt, f);
}
