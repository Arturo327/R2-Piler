#include "ir/opts/common.h"

static int strength_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	IRInstr *k;
	uint64_t v;

	if (in->dst == NO_REG || in->src2 == NO_REG) return 0;
	if (in->op != IR_MUL && in->op != IR_DIV && in->op != IR_MOD) return 0;
	if (f->defs[in->src2] != 1 || f->uses[in->src2] != 1) return 0;

	k = opt->ir->instrs + f->def_at[in->src2];
	if (k->op != IR_CONST) return 0;
	v = (uint64_t)norm_val((uint64_t)k->imm64, f->fn->reg_types[in->src2]);
	if (v < 2 || (v & (v - 1)) != 0) return 0;
	if (in->op != IR_MUL && types[in->data_type].sign) return 0;

	if (in->op == IR_MUL) {
		in->op = IR_LS;
		k->imm64 = __builtin_ctzll(v);
	} else if (in->op == IR_DIV) {
		in->op = IR_RS;
		k->imm64 = __builtin_ctzll(v);
	} else {
		in->op = IR_AND_A;
		k->imm64 = (int64_t)(v - 1);
	}
	return 1;
}

int opt_strength (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = 0; i < f->fn->count; i++)
		changed |= strength_instr(opt, f, code + i);

	return changed;
}

static int fwd_global (OptFn *f, IRInstr *in)
{
	if (f->gstamp[in->target] != f->gepoch) return 0;

	uint32_t r = f->gval[in->target];
	if (f->fn->reg_types[in->dst] != f->fn->reg_types[r]) return 0;

	in->op = IR_MOVE;
	in->src1 = r;
	in->src2 = NO_REG;
	in->data_type = f->fn->reg_types[r];
	return 1;
}

static void note_global (OptFn *f, IRInstr *in, uint32_t reg)
{
	uint32_t g = in->target;

	f->gstamp[g] = 0;
	if (f->defs[reg] != 1) return;
	if (f->fn->reg_types[reg] != in->data_type) return;
	f->gval[g] = reg;
	f->gstamp[g] = f->gepoch;
}

static int globals_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->gepoch++;
	for (uint32_t i = 0; i < blk->count; i++) {
		IRInstr *in = code + i;

		if (in->op == IR_CALL) f->gepoch++;
		else if (in->op == IR_STR_GLOBAL) note_global(f, in, in->src1);
		else if (in->op == IR_LD_GLOBAL && fwd_global(f, in)) changed = 1;
		else if (in->op == IR_LD_GLOBAL) note_global(f, in, in->dst);
	}
	return changed;
}

int opt_globals (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= globals_block(opt, f, f->blocks + b);
	return changed;
}

static int dse_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->depoch++;
	uint32_t i = blk->count;
	while (i-- > 0) {
		IRInstr *in = code + i;

		if (in->op == IR_NOP) continue;
		if (in->dst != NO_REG && f->dstamp[in->dst] == f->depoch
				&& is_deletable[in->op]) {
			kill_instr(in);
			changed = 1;
			continue;
		}
		if (in->dst != NO_REG) f->dstamp[in->dst] = f->depoch;
		if (in->src1 != NO_REG) f->dstamp[in->src1] = 0;
		if (in->src2 != NO_REG) f->dstamp[in->src2] = 0;
	}
	return changed;
}

int opt_dse (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= dse_block(opt, f, f->blocks + b);
	return changed;
}
