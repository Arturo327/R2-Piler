#include "ir/opts/common.h"

int try_move (OptFn *f, IRInstr *in, uint32_t src)
{
	if (!same_rep(f->fn, in->dst, src)) return 0;
	if (in->dst == src) {
		kill_instr(in);
		return 1;
	}
	in->op = IR_MOVE;
	in->src1 = src;
	in->src2 = NO_REG;
	return 1;
}

static int simplify_same (OptFn *f, IRInstr *in)
{
	switch (in->op)
	{
	case IR_SUB: case IR_XOR: case IR_NE: case IR_LT: case IR_GT:
		to_const(f, in, 0);
		return 1;
	case IR_EQ: case IR_LE: case IR_GE:
		to_const(f, in, 1);
		return 1;
	case IR_AND_A: case IR_OR_A:
		return try_move(f, in, in->src1);
	default:
		return 0;
	}
}

static int simplify_rhs (OptFn *f, IRInstr *in, int64_t b)
{
	switch (in->op)
	{
	case IR_ADD: case IR_SUB: case IR_OR_A: case IR_XOR: case IR_LS: case IR_RS:
		return b == 0 && try_move(f, in, in->src1);
	case IR_DIV:
		return b == 1 && try_move(f, in, in->src1);
	case IR_MOD:
		if (b != 1) return 0;
		to_const(f, in, 0);
		return 1;
	case IR_MUL:
		if (b != 0) return b == 1 && try_move(f, in, in->src1);
		to_const(f, in, 0);
		return 1;
	case IR_AND_A:
		if (b != 0) return b == norm_val(~(uint64_t)0, in->data_type)
				&& try_move(f, in, in->src1);
		to_const(f, in, 0);
		return 1;
	default:
		return 0;
	}
}

static int simplify_lhs (OptFn *f, IRInstr *in, int64_t a)
{
	if (in->op != IR_LS && in->op != IR_RS) return 0;
	if (a != 0) return 0;
	to_const(f, in, 0);
	return 1;
}

static const uint8_t swap_op[IR_COUNT] = {
	[IR_ADD] = IR_ADD, [IR_MUL] = IR_MUL, [IR_AND_A] = IR_AND_A,
	[IR_OR_A] = IR_OR_A, [IR_XOR] = IR_XOR, [IR_EQ] = IR_EQ, [IR_NE] = IR_NE,
	[IR_GT] = IR_LT, [IR_LT] = IR_GT, [IR_GE] = IR_LE, [IR_LE] = IR_GE
};

int simplify_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	if (in->dst == NO_REG || in->src1 == NO_REG || in->src2 == NO_REG) return 0;
	if (in->src1 == in->src2) return simplify_same(f, in);

	int64_t a = 0;
	int64_t b = 0;

	int ca = reg_const(opt, f, in->src1, &a);
	int cb = reg_const(opt, f, in->src2, &b);

	if (ca && cb) return 0;
	if (cb) return simplify_rhs(f, in, b);
	if (ca && swap_op[in->op]) {
		uint32_t tmp = in->src1;

		in->src1 = in->src2;
		in->src2 = tmp;
		in->op = swap_op[in->op];
		return 1;
	}
	if (ca) return simplify_lhs(f, in, a);
	return 0;
}
