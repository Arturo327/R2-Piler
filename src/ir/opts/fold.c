#include "ir/opts/common.h"

int reg_const (Optimizer *opt, OptFn *f, uint32_t reg, int64_t *val)
{
	IRInstr *def;

	if (f->kstamp[reg] == f->epoch) {
		*val = f->kval[reg];
		return 1;
	}
	if (f->defs[reg] != 1) return 0;
	def = opt->ir->instrs + f->def_at[reg];
	if (def->op != IR_CONST) return 0;

	*val = norm_val((uint64_t)def->imm64, f->fn->reg_types[reg]);
	return 1;
}

static int eval_divmod (uint8_t op, int sign, int64_t a, int64_t b, int64_t *r)
{
	if (b == 0 || (sign && b == -1)) return 0;
	if (sign) {
		*r = op == IR_DIV ? a / b : a % b;
		return 1;
	}
	if (op == IR_DIV) *r = (int64_t)((uint64_t)a / (uint64_t)b);
	else *r = (int64_t)((uint64_t)a % (uint64_t)b);
	return 1;
}

static int eval_compare (uint8_t op, int sign, int64_t a, int64_t b, int64_t *r)
{
	uint64_t ua = (uint64_t)a;
	uint64_t ub = (uint64_t)b;

	switch (op)
	{
	case IR_EQ: *r = a == b; return 1;
	case IR_NE: *r = a != b; return 1;
	case IR_GT: *r = sign ? a > b : ua > ub; return 1;
	case IR_GE: *r = sign ? a >= b : ua >= ub; return 1;
	case IR_LT: *r = sign ? a < b : ua < ub; return 1;
	case IR_LE: *r = sign ? a <= b : ua <= ub; return 1;
	default: return 0;
	}
}

static int eval_binary (uint8_t op, uint8_t type, int64_t a, int64_t b, int64_t *r)
{
	int sign = types[type].sign;
	uint64_t ua = (uint64_t)a;
	uint64_t ub = (uint64_t)b;

	switch (op)
	{
	case IR_ADD: *r = (int64_t)(ua + ub); return 1;
	case IR_SUB: *r = (int64_t)(ua - ub); return 1;
	case IR_MUL: *r = (int64_t)(ua * ub); return 1;
	case IR_AND_A: *r = (int64_t)(ua & ub); return 1;
	case IR_OR_A: *r = (int64_t)(ua | ub); return 1;
	case IR_XOR: *r = (int64_t)(ua ^ ub); return 1;
	case IR_LS:
		if (ub >= 64) return 0;
		*r = (int64_t)(ua << ub);
		return 1;
	case IR_RS:
		if (ub >= 64) return 0;
		*r = sign ? a >> ub : (int64_t)(ua >> ub);
		return 1;
	case IR_DIV: case IR_MOD: return eval_divmod(op, sign, a, b, r);
	default: return eval_compare(op, sign, a, b, r);
	}
}

static int eval_unary (uint8_t op, int64_t a, int64_t *r)
{
	switch (op)
	{
	case IR_MOVE: case IR_EXTEND: *r = a; return 1;
	case IR_NEG: *r = (int64_t)(0 - (uint64_t)a); return 1;
	case IR_NOT_A: *r = ~a; return 1;
	case IR_NOT_L: *r = a == 0; return 1;
	default: return 0;
	}
}

void to_const (OptFn *f, IRInstr *in, int64_t v)
{
	in->data_type = f->fn->reg_types[in->dst];
	in->imm64 = norm_val((uint64_t)v, in->data_type);
	in->op = IR_CONST;
	in->src1 = NO_REG;
	in->src2 = NO_REG;
}

static int fold_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	int64_t a, b, r = 0;
	int ok = 0;

	if (in->dst == NO_REG || in->src1 == NO_REG) return 0;
	if (!reg_const(opt, f, in->src1, &a)) return 0;

	if (in->src2 == NO_REG) ok = eval_unary(in->op, a, &r);
	else if (reg_const(opt, f, in->src2, &b))
		ok = eval_binary(in->op, in->data_type, a, b, &r);
	if (!ok) return 0;

	to_const(f, in, r);
	return 1;
}

static int fold_jump (Optimizer *opt, OptFn *f, IRInstr *in)
{
	int64_t v;

	if (!reg_const(opt, f, in->src1, &v)) return 0;
	if ((v != 0) == (in->op == IR_JNZ)) {
		in->op = IR_JMP;
		in->src1 = NO_REG;
	} else kill_instr(in);
	return 1;
}

static void track_const (OptFn *f, IRInstr *in)
{
	if (in->dst == NO_REG) return;
	if (in->op != IR_CONST) {
		f->kstamp[in->dst] = 0;
		return;
	}
	f->kstamp[in->dst] = f->epoch;
	f->kval[in->dst] = norm_val((uint64_t)in->imm64, f->fn->reg_types[in->dst]);
}

static int fold_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->epoch++;
	for (uint32_t i = 0; i < blk->count; i++) {
		IRInstr *in = code + i;

		if (in->op == IR_JZ || in->op == IR_JNZ)
			changed |= fold_jump(opt, f, in);
		else changed |= fold_instr(opt, f, in) || simplify_instr(opt, f, in);
		track_const(f, in);
	}
	return changed;
}

int opt_fold (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= fold_block(opt, f, f->blocks + b);
	f->epoch++;
	return changed;
}
