#include "ir/opts/common.h"

#include <stdlib.h>
#include <string.h>

#define SROA_NONE 0xFFFFFFFFu

typedef struct Access {
	int64_t off;
	uint32_t slot;
	uint32_t reg;
	TypeId type;
} Access;

typedef struct Sroa {
	IRFn *fn;
	IRInstr *code;
	Arena *arena;
	uint8_t *defs;
	uint8_t *isk;
	uint8_t *bad;
	int64_t *kval;
	int64_t *roff;
	uint32_t *root;
	Access *acc;
	uint32_t acc_n;
	uint32_t mem_n;
} Sroa;

static int acc_cmp (const void *pa, const void *pb)
{
	const Access *a = pa;
	const Access *b = pb;

	if (a->slot != b->slot) return a->slot < b->slot ? -1 : 1;
	if (a->off != b->off) return a->off < b->off ? -1 : 1;
	return 0;
}

static inline void mark_bad (Sroa *s, uint32_t reg)
{
	if ((reg) != NO_REG && (s)->root[reg] != SROA_NONE)
		(s)->bad[(s)->root[reg]] = 1;
}

static void scan_defs (Sroa *s)
{
	IRFn *fn = s->fn;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = s->code + i;

		if (in->op == IR_LD_PTR || in->op == IR_STR_PTR) s->mem_n++;
		if (in->dst == NO_REG) continue;
		if (s->defs[in->dst] < 2) s->defs[in->dst]++;
		s->isk[in->dst] = in->op == IR_CONST;
		if (in->op == IR_CONST)
			s->kval[in->dst] = norm_val((uint64_t)in->imm64, fn->reg_types[in->dst]);
	}
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (s->defs[r] != 1) s->isk[r] = 0;
	}
}

static void derive_addr (Sroa *s, IRInstr *in)
{
	if (type_kind(s->fn->reg_types[in->src1]) != TK_STRUCT) return;
	if (s->defs[in->dst] != 1 || !reg_addressed(s->fn, in->src1)) {
		s->bad[in->src1] = 1;
		return;
	}
	s->root[in->dst] = in->src1;
	s->roff[in->dst] = 0;
}

static void derive_add (Sroa *s, IRInstr *in)
{
	int64_t sign = in->op == IR_SUB ? -1 : 1;
	uint32_t x = in->src1;
	uint32_t y = in->src2;
	int ok = s->defs[in->dst] == 1 && types[in->data_type].size == 8;

	if (ok && s->root[x] != SROA_NONE && s->isk[y]) {
		s->root[in->dst] = s->root[x];
		s->roff[in->dst] = s->roff[x] + sign * s->kval[y];
		return;
	}
	if (ok && in->op == IR_ADD && s->root[y] != SROA_NONE && s->isk[x]) {
		s->root[in->dst] = s->root[y];
		s->roff[in->dst] = s->roff[y] + s->kval[x];
		return;
	}
	mark_bad(s, x);
	mark_bad(s, y);
}

static void derive_move (Sroa *s, IRInstr *in)
{
	uint32_t x = in->src1;

	if (s->root[x] == SROA_NONE) return;
	if (s->defs[in->dst] != 1 || types[s->fn->reg_types[in->dst]].size != 8) {
		mark_bad(s, x);
		return;
	}
	s->root[in->dst] = s->root[x];
	s->roff[in->dst] = s->roff[x];
}

static void note_access (Sroa *s, IRInstr *in)
{
	Access *a;

	if (s->root[in->src1] == SROA_NONE) return;
	a = s->acc + s->acc_n++;
	a->slot = s->root[in->src1];
	a->off = s->roff[in->src1] + in->imm64;
	a->type = in->data_type;
}

static void trace_uses (Sroa *s)
{
	for (uint32_t i = 0; i < s->fn->count; i++) {
		IRInstr *in = s->code + i;
		uint32_t *src[3];
		int n;

		if (in->op == IR_ADDR_LOCAL || in->op == IR_ADD || in->op == IR_SUB
				|| in->op == IR_MOVE) continue;
		if (in->op == IR_LD_PTR || in->op == IR_STR_PTR) {
			note_access(s, in);
			if (in->op == IR_STR_PTR) mark_bad(s, in->src2);
			continue;
		}
		n = get_srcs(in, src);
		for (int k = 0; k < n; k++)
			mark_bad(s, *src[k]);
	}
}

static void derive_all (Sroa *s)
{
	for (uint32_t i = 0; i < s->fn->count; i++) {
		IRInstr *in = s->code + i;

		if (in->op == IR_ADDR_LOCAL) derive_addr(s, in);
		else if (in->op == IR_ADD || in->op == IR_SUB) derive_add(s, in);
		else if (in->op == IR_MOVE) derive_move(s, in);
	}
}

static void validate (Sroa *s)
{
	qsort(s->acc, s->acc_n, sizeof(Access), acc_cmp);
	for (uint32_t i = 0; i < s->acc_n; i++) {
		Access *a = s->acc + i;
		const Access *p = i ? a - 1 : a;

		if (type_is_aggregate(a->type)) s->bad[a->slot] = 1;
		if (!i || p->slot != a->slot) continue;
		if (p->off == a->off && p->type != a->type) s->bad[a->slot] = 1;
		if (p->off != a->off && p->off + (int64_t)types[p->type].size > a->off)
			s->bad[a->slot] = 1;
	}
}

static uint32_t assign_regs (Sroa *s)
{
	uint32_t next = s->fn->reg_count;

	for (uint32_t i = 0; i < s->acc_n; i++) {
		Access *a = s->acc + i;
		int same = i && a[-1].slot == a->slot && a[-1].off == a->off;

		if (s->bad[a->slot]) continue;
		a->reg = same ? a[-1].reg : next++;
	}
	return next;
}

static void grow_regs (Sroa *s, uint32_t total)
{
	IRFn *fn = s->fn;
	TypeId *types_new = arena_alloc(s->arena, (size_t)total * sizeof(TypeId));
	uint8_t *addr_new = arena_alloc(s->arena, total);

	memcpy(types_new, fn->reg_types, (size_t)fn->reg_count * sizeof(TypeId));
	memset(addr_new, 0, total);
	memcpy(addr_new, fn->addressed, fn->reg_count);
	for (uint32_t i = 0; i < s->acc_n; i++) {
		if (!s->bad[s->acc[i].slot]) types_new[s->acc[i].reg] = s->acc[i].type;
	}
	fn->reg_types = types_new;
	fn->addressed = addr_new;
	fn->reg_count = total;
}

static void rewrite (Sroa *s)
{
	for (uint32_t i = 0; i < s->fn->count; i++) {
		IRInstr *in = s->code + i;
		Access key = {0};
		if ((in->op != IR_LD_PTR && in->op != IR_STR_PTR)
				|| s->root[in->src1] == SROA_NONE) continue;

		key.slot = s->root[in->src1];
		key.off = s->roff[in->src1] + in->imm64;
		if (s->bad[key.slot]) continue;

		Access *a = bsearch(&key, s->acc, s->acc_n, sizeof(Access), acc_cmp);
		if (in->op == IR_LD_PTR) {
			in->src1 = a->reg;
		} else {
			in->dst = a->reg;
			in->src1 = in->src2;
		}
		in->op = IR_MOVE;
		in->src2 = NO_REG;
		in->imm64 = 0;
	}
}

static void release_slots (Sroa *s)
{
	for (uint32_t i = 0; i < s->fn->count; i++) {
		IRInstr *in = s->code + i;

		if (in->op != IR_ADDR_LOCAL || s->root[in->dst] == SROA_NONE) continue;
		if (s->bad[s->root[in->dst]]) continue;
		s->fn->addressed[s->root[in->dst]] = 0;
		kill_instr(in);
	}
}

static void sroa_fn (IR *ir, IRFn *fn, Arena *a)
{
	size_t n = (size_t)fn->reg_count + 1;
	Sroa s = { .fn = fn, .code = ir->instrs + fn->start, .arena = a };

	s.defs = arena_alloc(a, n);
	s.isk = arena_alloc(a, n);
	s.bad = arena_alloc(a, n);
	s.kval = arena_alloc(a, n * sizeof(int64_t));
	s.roff = arena_alloc(a, n * sizeof(int64_t));
	s.root = arena_alloc(a, n * sizeof(uint32_t));
	memset(s.defs, 0, n);
	memset(s.isk, 0, n);
	memset(s.bad, 0, n);
	memset(s.root, 0xFF, n * sizeof(uint32_t));

	scan_defs(&s);
	s.acc = arena_alloc(a, (s.mem_n + 1) * sizeof(Access));
	derive_all(&s);
	trace_uses(&s);
	validate(&s);

	uint32_t total = assign_regs(&s);
	if (total == fn->reg_count) return;
	grow_regs(&s, total);
	rewrite(&s);
	release_slots(&s);
}

void sroa_ir (IR *ir, Arena *a)
{
	for (uint32_t i = 0; i < ir->fn_count; i++) {
		IRFn *fn = ir->fns + i;

		if (fn->is_extern || !fn->count || !fn->addressed) continue;
		sroa_fn(ir, fn, a);
	}
}
