#include "codegen/parallel.h"

#define PM_MAX_REGS 64

typedef struct PMPlan {
	PMove regs[PM_MAX_REGS];
	PMAction *out;
	uint64_t done;
	uint32_t count;
	uint32_t actions;
} PMPlan;

static void action_from_move (PMAction *a, PMove *m, uint8_t kind)
{
	a->kind = kind;
	a->from = m->from;
	a->to = m->to;
	a->size = m->size;
	a->sign = m->sign;
	a->slot = m->slot;
}

static uint32_t plan_stores (PMove *moves, uint32_t count, PMAction *out)
{
	uint32_t n = 0;

	for (uint32_t i = 0; i < count; i++) {
		PMove *m = moves + i;

		if (m->from < 0 || m->to >= 0) continue;
		action_from_move(out + n, m, PM_ACT_STORE);
		n++;
	}
	return n;
}

static uint32_t plan_loads (PMove *moves, uint32_t count, PMAction *out)
{
	uint32_t n = 0;

	for (uint32_t i = 0; i < count; i++) {
		PMove *m = moves + i;

		if (m->from >= 0 || m->to < 0) continue;
		action_from_move(out + n, m, PM_ACT_LOAD);
		n++;
	}
	return n;
}

static uint32_t collect_regs (PMove *moves, uint32_t count, PMove *regs)
{
	uint32_t n = 0;

	for (uint32_t i = 0; i < count; i++) {
		PMove *m = moves + i;

		if (m->from < 0 || m->to < 0 || m->from == m->to) continue;
		if (n == PM_MAX_REGS) break;
		regs[n++] = *m;
	}
	return n;
}

static int source_pending (PMPlan *p, uint32_t j)
{
	for (uint32_t k = 0; k < p->count; k++) {
		if (k == j || (p->done >> k & 1)) continue;
		if (p->regs[k].from == p->regs[j].to) return 1;
	}
	return 0;
}

static int ready_reg (PMPlan *p)
{
	for (uint32_t j = 0; j < p->count; j++) {
		if (p->done >> j & 1) continue;
		if (p->regs[j].from == p->regs[j].to) return (int)j;
		if (!source_pending(p, j)) return (int)j;
	}
	return -1;
}

static uint32_t first_pending (PMPlan *p)
{
	for (uint32_t j = 0; j < p->count; j++)
		if (!(p->done >> j & 1)) return j;
	return 0;
}

static void add_reg_action (PMPlan *p, int8_t from, int8_t to, uint8_t kind)
{
	PMAction *a = p->out + p->actions;

	a->kind = kind;
	a->from = from;
	a->to = to;
	a->size = 8;
	a->sign = 0;
	a->slot = 0;
	p->actions++;
}

static void swap_cycle (PMPlan *p, uint32_t j)
{
	int8_t from = p->regs[j].from;
	int8_t to = p->regs[j].to;

	add_reg_action(p, from, to, PM_ACT_SWAP);
	for (uint32_t k = 0; k < p->count; k++) {
		if (k == j || (p->done >> k & 1)) continue;
		if (p->regs[k].from == to) p->regs[k].from = from;
		else if (p->regs[k].from == from) p->regs[k].from = to;
	}
	p->done |= 1ull << j;
}

static void plan_regs (PMove *moves, uint32_t count, PMPlan *p)
{
	p->count = collect_regs(moves, count, p->regs);
	p->done = 0;
	p->actions = 0;
	uint64_t full = p->count == PM_MAX_REGS ? ~0ull : (1ull << p->count) - 1;

	while (p->done != full) {
		int j = ready_reg(p);

		if (j < 0) {
			swap_cycle(p, first_pending(p));
			continue;
		}
		if (p->regs[j].from != p->regs[j].to)
			add_reg_action(p, p->regs[j].from, p->regs[j].to, PM_ACT_MOVE);
		p->done |= 1ull << j;
	}
}

uint32_t parallel_actions (PMove *moves, uint32_t count, PMAction *out)
{
	PMPlan plan;
	uint32_t n = plan_stores(moves, count, out);

	plan.out = out + n;
	plan_regs(moves, count, &plan);
	n += plan.actions;
	n += plan_loads(moves, count, out + n);
	return n;
}
