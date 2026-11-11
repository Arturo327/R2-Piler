#include "ir/opts/common.h"

static void fwd_key (Optimizer *opt, OptFn *f, IRInstr *in, FwdEntry *key)
{
	key->off = in->imm64;
	key->type = in->data_type;
	key->base = in->src1;
	key->kind = FWD_PTR;
	if (in->op == IR_LD_LOCAL || in->op == IR_STR_LOCAL) {
		key->kind = FWD_SLOT;
		return;
	}
	if (f->defs[in->src1] != 1) return;
	IRInstr *def = opt->ir->instrs + f->def_at[in->src1];
	if (def->op != IR_ADDR_GLOBAL) return;
	key->kind = FWD_GLOB;
	key->base = def->target;
}

static FwdEntry *fwd_find (OptFn *f, const FwdEntry *key)
{
	uint32_t size = types[key->type].size;

	for (uint32_t k = 0; k < f->fwd_count; k++) {
		FwdEntry *e = f->fwd + k;

		if (e->kind == key->kind && e->base == key->base
				&& e->off == key->off && types[e->type].size == size)
			return e;
	}
	return NULL;
}

static int fwd_survives (const FwdEntry *e, const FwdEntry *key)
{
	int64_t e_end = e->off + (int64_t)types[e->type].size;
	int64_t k_end = key->off + (int64_t)types[key->type].size;
	int disjoint = e->off >= k_end || key->off >= e_end;

	if (key->kind == FWD_PTR)
		return e->kind == FWD_PTR && e->base == key->base && disjoint;
	if (e->kind == FWD_PTR) return 0;
	return e->kind != key->kind || e->base != key->base || disjoint;
}

static void fwd_kill_store (OptFn *f, const FwdEntry *key)
{
	uint32_t keep = 0;

	for (uint32_t k = 0; k < f->fwd_count; k++) {
		if (!fwd_survives(f->fwd + k, key)) continue;
		f->fwd[keep++] = f->fwd[k];
	}
	f->fwd_count = keep;
}

static void fwd_kill_reg (OptFn *f, uint32_t reg)
{
	uint32_t keep = 0;

	for (uint32_t k = 0; k < f->fwd_count; k++) {
		FwdEntry *e = f->fwd + k;
		int based = e->kind != FWD_GLOB && e->base == reg;

		if (based || e->val == reg) continue;
		f->fwd[keep++] = *e;
	}
	f->fwd_count = keep;
}

static void fwd_kill_global (OptFn *f, uint32_t g)
{
	uint32_t keep = 0;

	for (uint32_t k = 0; k < f->fwd_count; k++) {
		FwdEntry *e = f->fwd + k;
		int same = e->kind == FWD_GLOB && e->base == g;

		if (e->kind == FWD_PTR || same) continue;
		f->fwd[keep++] = *e;
	}
	f->fwd_count = keep;
}

static void fwd_add (OptFn *f, const FwdEntry *key, uint32_t val)
{
	if (f->fwd_count >= OPT_FWD_MAX || reg_addressed(f->fn, val)) return;
	if (types[f->fn->reg_types[val]].size != types[key->type].size) return;
	FwdEntry *e = f->fwd + f->fwd_count++;
	*e = *key;
	e->val = val;
}

static int fwd_load (Optimizer *opt, OptFn *f, IRInstr *ld)
{
	FwdEntry key;
	fwd_key(opt, f, ld, &key);
	FwdEntry *e = fwd_find(f, &key);
	if (!e || reg_addressed(f->fn, ld->dst)) return 0;
	if (!try_move(f, ld, e->val)) return 0;
	ld->data_type = f->fn->reg_types[e->val];
	ld->imm64 = 0;
	return 1;
}

static void fwd_remember (Optimizer *opt, OptFn *f, IRInstr *ld)
{
	FwdEntry key;
	fwd_key(opt, f, ld, &key);
	if (key.kind != FWD_GLOB && key.base == ld->dst) return;
	fwd_add(f, &key, ld->dst);
}

static void fwd_store (Optimizer *opt, OptFn *f, IRInstr *st)
{
	FwdEntry key;
	fwd_key(opt, f, st, &key);
	fwd_kill_store(f, &key);
	fwd_add(f, &key, st->src2);
}

static int fields_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	int load = in->op == IR_LD_PTR || in->op == IR_LD_LOCAL;
	int changed = load && fwd_load(opt, f, in);

	if (in->dst != NO_REG) fwd_kill_reg(f, in->dst);
	switch (in->op)
	{
	case IR_STR_PTR: case IR_STR_LOCAL: fwd_store(opt, f, in); break;
	case IR_LD_PTR: case IR_LD_LOCAL: fwd_remember(opt, f, in); break;
	case IR_STR_GLOBAL: fwd_kill_global(f, in->target); break;
	case IR_CALL: f->fwd_count = 0; break;
	default: break;
	}
	return changed;
}

int opt_fields (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++) {
		CfgBlock *blk = f->blocks + b;
		IRInstr *code = opt->ir->instrs + blk->start;

		f->fwd_count = 0;
		for (uint32_t i = 0; i < blk->count; i++) {
			if (code[i].op != IR_NOP) changed |= fields_instr(opt, f, code + i);
		}
	}
	return changed;
}
