#include "ir/opts/common.h"

#include <string.h>

#define INLINE_MAX_INSTRS 24

typedef struct Inliner {
	IR *ir;
	Arena *arena;
	IRFn *fns;
	IRInstr *src;
	IRInstr *out;
	uint8_t *elig;
	uint8_t *types;
	uint32_t count;
	uint32_t regs;
} Inliner;

typedef struct Site {
	const uint32_t *args;
	uint32_t base;
	uint32_t lbase;
	uint32_t llo;
	uint32_t end;
	uint32_t dst;
} Site;

static IRInstr inl_instr (uint8_t op, uint32_t dst, uint32_t src, uint8_t type)
{
	IRInstr i = { .dst = dst, .src1 = src, .src2 = NO_REG };

	i.op = op;
	i.data_type = type;
	return i;
}

static void mark_eligible (Inliner *n)
{
	for (uint32_t i = 0; i < n->ir->fn_count; i++) {
		IRFn *fn = n->fns + i;
		int ok = i != n->ir->init_fn && fn->count <= INLINE_MAX_INSTRS;

		for (uint32_t j = 0; ok && j < fn->count; j++)
			ok = n->src[fn->start + j].op != IR_CALL;
		n->elig[i] = (uint8_t)ok;
	}
}

static void inl_measure (Inliner *n, IRFn *fn, uint32_t *instrs, uint32_t *regs)
{
	*instrs = fn->count;
	*regs = fn->reg_count;
	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = n->src + fn->start + j;
		IRFn *callee;

		if (in->op != IR_CALL || !n->elig[in->target]) continue;
		callee = n->fns + in->target;
		*instrs += 2 * callee->count + 1;
		*regs += callee->reg_count;
	}
}

static void label_span (Inliner *n, IRFn *fn, uint32_t *lo, uint32_t *hi)
{
	*lo = UINT32_MAX;
	*hi = 0;
	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *c = n->src + fn->start + j;

		if (c->op < IR_LABEL || c->op > IR_JNZ) continue;
		if (c->target < *lo) *lo = c->target;
		if (c->target > *hi) *hi = c->target;
	}
}

static void inl_ret (Inliner *n, const Site *s, IRInstr *c, int last)
{
	IRInstr j;

	if (c->src1 != NO_REG && s->dst != NO_REG)
		n->out[n->count++] = inl_instr(IR_MOVE, s->dst, s->base + c->src1, c->data_type);
	if (last) return;
	j = inl_instr(IR_JMP, NO_REG, NO_REG, TYPE_VOID);
	j.target = s->end;
	n->out[n->count++] = j;
}

static void inl_body (Inliner *n, const Site *s, IRInstr *c, int last)
{
	IRInstr o = *c;

	if (c->op == IR_RET) {
		inl_ret(n, s, c, last);
		return;
	}
	if (c->op == IR_PARAM) {
		n->out[n->count++] = inl_instr(IR_MOVE, s->base + c->dst,
				s->args[c->target], c->data_type);
		return;
	}

	if (o.dst != NO_REG) o.dst += s->base;
	uint32_t *src[3];
	int m = get_srcs(&o, src);
	for (int k = 0; k < m; k++)
		*src[k] += s->base;

	if (o.op >= IR_LABEL && o.op <= IR_JNZ)
		o.target = s->lbase + o.target - s->llo;
	n->out[n->count++] = o;
}

static void inl_site (Inliner *n, IRInstr *call, const uint32_t *args)
{
	IRFn *callee = n->fns + call->target;
	Site s = { .base = n->regs, .dst = call->dst, .args = args };
	IRInstr end = inl_instr(IR_LABEL, NO_REG, NO_REG, TYPE_VOID);

	uint32_t hi;
	uint32_t span = 0;

	label_span(n, callee, &s.llo, &hi);
	if (s.llo <= hi) span = hi - s.llo + 1;
	s.lbase = n->ir->label_count;
	s.end = s.lbase + span;

	n->ir->label_count = s.end + 1;
	if (callee->reg_count)
		memcpy(n->types + n->regs, callee->reg_types, callee->reg_count);

	n->regs += callee->reg_count;
	for (uint32_t j = 0; j < callee->count; j++)
		inl_body(n, &s, n->src + callee->start + j, j + 1 == callee->count);

	end.target = s.end;
	n->out[n->count++] = end;
}

static int inl_call (Inliner *n, IRInstr *call)
{
	uint32_t argc = call->argc;
	uint32_t args[argc + 1];
	IRInstr *first;

	if (n->count < argc) return 0;
	first = n->out + (n->count - argc);
	for (uint32_t k = 0; k < argc; k++) {
		if (first[k].op != IR_ARG || first[k].target != k) return 0;
		args[k] = first[k].src1;
	}

	n->count -= argc;
	inl_site(n, call, args);
	return 1;
}

static void inl_fn (Inliner *n, uint32_t idx)
{
	IRFn *fn = n->ir->fns + idx;
	IRFn *old = n->fns + idx;
	uint32_t instrs;
	uint32_t regs;

	inl_measure(n, old, &instrs, &regs);
	n->types = old->reg_types;
	if (regs != old->reg_count) {
		n->types = arena_alloc(n->arena, regs);
		if (old->reg_count) memcpy(n->types, old->reg_types, old->reg_count);
	}

	n->regs = old->reg_count;
	fn->start = n->count;
	for (uint32_t j = 0; j < old->count; j++) {
		IRInstr *in = n->src + old->start + j;

		if (in->op == IR_CALL && n->elig[in->target] && inl_call(n, in)) continue;
		n->out[n->count++] = *in;
	}

	fn->count = n->count - fn->start;
	fn->reg_count = n->regs;
	fn->reg_types = n->types;
}

int inline_ir (IR *ir, Arena *a)
{
	Inliner n = { .ir = ir, .arena = a, .src = ir->instrs };
	uint32_t total = 0, instrs, regs;

	n.fns = arena_alloc(a, ir->fn_count * sizeof(IRFn));
	memcpy(n.fns, ir->fns, ir->fn_count * sizeof(IRFn));
	n.elig = arena_alloc(a, ir->fn_count);
	mark_eligible(&n);

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		inl_measure(&n, n.fns + i, &instrs, &regs);
		total += instrs;
	}
	if (total == ir->instr_count) return 0;

	n.out = arena_alloc(a, (size_t)total * sizeof(IRInstr));
	for (uint32_t i = 0; i < ir->fn_count; i++)
		inl_fn(&n, i);
	ir->instrs = n.out;
	ir->instr_count = n.count;
	ir->instr_cap = total;
	return 1;
}
