#include "ir/opts/common.h"

int opt_coalesce (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = 0; i < f->fn->count; i++) {
		IRInstr *a = code + i;
		IRInstr *m;
		int can = (is_deletable[a->op] && a->op != IR_MOVE)
				|| a->op == IR_CALL || a->op == IR_DIV || a->op == IR_MOD;

		if (!can || a->dst == NO_REG) continue;
		if (f->defs[a->dst] != 1 || f->uses[a->dst] != 1) continue;

		m = next_real(code, i + 1, f->fn->count);
		if (!m || m->op != IR_MOVE || m->src1 != a->dst || m->dst == a->dst) continue;
		if (!same_rep(f->fn, a->dst, m->dst)) continue;

		a->dst = m->dst;
		kill_instr(m);
		changed = 1;
	}
	return changed;
}

static const uint8_t cse_cost[IR_COUNT] = {
	[IR_MUL] = 1, [IR_DIV] = 1, [IR_MOD] = 1
};

static void cse_kill (OptFn *f, uint32_t reg)
{
	uint32_t keep = 0;

	for (uint32_t k = 0; k < f->cse_count; k++) {
		CseEntry *e = f->cse + k;

		if (e->dst == reg || e->src1 == reg || e->src2 == reg) continue;
		f->cse[keep++] = *e;
	}
	f->cse_count = keep;
}

static CseEntry *cse_find (OptFn *f, IRInstr *in)
{
	for (uint32_t k = 0; k < f->cse_count; k++) {
		CseEntry *e = f->cse + k;
		int same = e->src1 == in->src1 && e->src2 == in->src2;
		int swapped = in->op == IR_MUL && e->src1 == in->src2 && e->src2 == in->src1;

		if (e->op == in->op && e->type == in->data_type && (same || swapped))
			return e;
	}
	return NULL;
}

static void cse_add (OptFn *f, IRInstr *in)
{
	CseEntry *e;

	if (f->cse_count >= OPT_CSE_MAX) return;
	if (in->dst == in->src1 || in->dst == in->src2) return;
	e = f->cse + f->cse_count++;
	e->dst = in->dst;
	e->src1 = in->src1;
	e->src2 = in->src2;
	e->op = in->op;
	e->type = in->data_type;
}

static int cse_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->cse_count = 0;
	for (uint32_t i = 0; i < blk->count; i++) {
		IRInstr *in = code + i;
		int64_t k;
		int cand = cse_cost[in->op] && in->dst != NO_REG
				&& !reg_const(opt, f, in->src2, &k);
		CseEntry *e = cand ? cse_find(f, in) : NULL;

		if (e && same_rep(f->fn, in->dst, e->dst)) {
			in->op = IR_MOVE;
			in->src1 = e->dst;
			in->src2 = NO_REG;
			in->data_type = f->fn->reg_types[in->dst];
			changed = 1;
			cand = 0;
		}
		if (in->dst != NO_REG) cse_kill(f, in->dst);
		if (cand) cse_add(f, in);
	}
	return changed;
}

int opt_cse (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= cse_block(opt, f, f->blocks + b);
	return changed;
}
