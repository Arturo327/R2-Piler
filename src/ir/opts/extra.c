#include "ir/opts/common.h"

#include <string.h>

static int strength_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	IRInstr *k;
	uint64_t v;

	if (in->dst == NO_REG || in->src2 == NO_REG) return 0;
	if (in->op != IR_MUL && in->op != IR_DIV && in->op != IR_MOD) return 0;
	if (reg_addressed(f->fn, in->src2)) return 0;
	if (f->defs[in->src2] != 1 || f->uses[in->src2] != 1) return 0;
	if (f->def_at[in->src2] - f->fn->start >= f->entry_end) return 0;

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

uint32_t visible_globals (IR *ir, Arena *a, uint32_t *out)
{
	uint8_t *seen = arena_alloc(a, (size_t)ir->global_count + 1);
	uint32_t n = 0;

	memset(seen, 0, (size_t)ir->global_count + 1);
	for (uint32_t g = 0; g < ir->global_count; g++)
		seen[g] = ir->globals[g].is_extern;
	for (uint32_t i = 0; i < ir->instr_count; i++) {
		IRInstr *in = ir->instrs + i;

		if (in->op == IR_ADDR_GLOBAL) seen[in->target] = 1;
	}
	for (uint32_t g = 0; g < ir->global_count; g++) {
		if (seen[g]) out[n++] = g;
	}
	return n;
}

static void kill_visible (OptFn *f)
{
	for (uint32_t k = 0; k < f->gvis_count; k++)
		f->gstamp[f->gvis[k]] = 0;
}

static int globals_block (Optimizer *opt, OptFn *f, CfgBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->gepoch++;
	for (uint32_t i = 0; i < blk->count; i++) {
		IRInstr *in = code + i;

		if (in->op == IR_CALL) f->gepoch++;
		else if (in->op == IR_STR_PTR) kill_visible(f);
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

static int dse_block (Optimizer *opt, OptFn *f, CfgBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->depoch++;
	uint32_t i = blk->count;
	while (i-- > 0) {
		IRInstr *in = code + i;

		if (in->op == IR_NOP) continue;
		if (in->op == IR_STR_PTR) f->depoch++;
		if (in->dst != NO_REG && f->dstamp[in->dst] == f->depoch
				&& is_deletable[in->op] && !reg_addressed(f->fn, in->dst)) {
			kill_instr(in);
			changed = 1;
			continue;
		}

		if (in->dst != NO_REG) f->dstamp[in->dst] = f->depoch;

		uint32_t *src[3];
		int n = get_srcs(in, src);
		for (int k = 0; k < n; k++)
			f->dstamp[*src[k]] = 0;
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
