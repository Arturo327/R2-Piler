#include "codegen/x86-64/common.h"

static const char *arith_ops[IR_COUNT] = {
	[IR_ADD] = "add",
	[IR_SUB] = "sub",
	[IR_MUL] = "imul",
	[IR_AND_A] = "and",
	[IR_OR_A] = "or",
	[IR_XOR] = "xor"
};

uint64_t x86_shift_amt (uint64_t v)
{
	uint64_t k = 0;

	while ((v & 1u) == 0) {
		v >>= 1;
		k++;
	}
	return k;
}

static void make_mul_pm1 (X86Fn *f, uint8_t w, uint64_t k, int plus)
{
	CodeGen *c = f->cg;
	uint8_t t = x86_grab(f);
	const char *wn = reg_name(w, 8);
	const char *tn = reg_name(t, 8);

	cg_printf(c, "\tmovq %s, %s\n\tshlq $%llu, %s\n", wn, tn, (unsigned long long)k, tn);
	if (plus) cg_printf(c, "\taddq %s, %s\n", tn, wn);
	else cg_printf(c, "\tsubq %s, %s\n\tmovq %s, %s\n", wn, tn, tn, wn);
}

static int make_mul_const (X86Fn *f, IRInstr *i)
{
	if (f->regs[i->src2].cstate != VR_CONST) return 0;
	int64_t v = x86_const_ext(f, i->src2);
	uint8_t w = x86_work_reg(f, i->dst, 0);
	const char *wn = reg_name(w, 8);
	x86_load_reg(f, w, i->src1);

	if (v > 0 && (v & (v - 1)) == 0)
		cg_printf(f->cg, "\tshlq $%llu, %s\n",
				(unsigned long long)x86_shift_amt((uint64_t)v), wn);
	else if (v == 3 || v == 5 || v == 9)
		cg_printf(f->cg, "\tleaq (%s,%s,%lld), %s\n", wn, wn, (long long)(v - 1), wn);
	else if (v > 9 && !((uint64_t)(v - 1) & (uint64_t)(v - 2)))
		make_mul_pm1(f, w, x86_shift_amt((uint64_t)(v - 1)), 1);
	else if (v > 3 && !((uint64_t)v & ((uint64_t)v + 1)))
		make_mul_pm1(f, w, x86_shift_amt((uint64_t)v + 1), 0);
	else x86_op_reg(f, "imul", w, i->src2);

	x86_store_reg(f, w, i->dst);
	return 1;
}

static int make_arith_inplace (X86Fn *f, IRInstr *i)
{
	int pb = x86_phys_reg(f, i->src2);
	int pd = x86_phys_reg(f, i->dst);

	if (pd < 0 || pd != pb) return 0;
	if (!(i->op == IR_ADD || i->op == IR_MUL || i->op == IR_AND_A ||
		i->op == IR_OR_A || i->op == IR_XOR)) return 0;

	x86_op_reg(f, arith_ops[i->op], (uint8_t)pd, i->src1);
	x86_store_reg(f, (uint8_t)pd, i->dst);
	return 1;
}

static int make_lea (X86Fn *f, IRInstr *i)
{
	if (i->op != IR_ADD || types[i->data_type].size != 8) return 0;

	int pa = x86_phys_reg(f, i->src1);
	int pb = x86_phys_reg(f, i->src2);
	int pd = x86_phys_reg(f, i->dst);
	if (pa < 0 || pb < 0 || pd < 0) return 0;

	cg_printf(f->cg, "\tleaq (%s,%s), %s\n",
			reg_name((uint8_t)pa, 8), reg_name((uint8_t)pb, 8),
			reg_name((uint8_t)pd, 8));
	return 1;
}

void x86_make_arith (X86Fn *f, IRInstr *i)
{
	if (i->op == IR_MUL && make_mul_const(f, i)) return;
	if (make_arith_inplace(f, i)) return;
	if (make_lea(f, i)) return;

	uint8_t w = x86_work_reg(f, i->dst, x86_home_bit(f, i->src2));

	x86_load_reg(f, w, i->src1);
	x86_op_reg(f, arith_ops[i->op], w, i->src2);
	x86_store_reg(f, w, i->dst);
}

void x86_make_shift (X86Fn *f, IRInstr *i)
{
	const char *op = "shl";
	int imm = f->regs[i->src2].cstate == VR_CONST;
	int64_t v = imm ? x86_const_ext(f, i->src2) & 63 : 0;
	int early = !imm && x86_phys_reg(f, i->src1) != RCX;
	uint64_t avoid = 0;

	if (i->op == IR_RS) op = types[i->data_type].sign ? "sar" : "shr";
	if (early) {
		x86_take_fixed(f, RCX, i->src2);
		x86_load_reg(f, RCX, i->src2);
		avoid = 1ull << RCX;
	} else if (!imm) {
		x86_take_fixed(f, RCX, i->src2);
		avoid = x86_home_bit(f, i->src2) | (1ull << RCX);
	}

	uint8_t w = x86_work_reg(f, i->dst, avoid);
	x86_load_reg(f, w, i->src1);

	if (imm) {
		cg_printf(f->cg, "\t%sq $%lld, %s\n", op, (long long)v, reg_name(w, 8));
	} else {
		if (!early) x86_load_reg(f, RCX, i->src2);
		cg_printf(f->cg, "\t%sq %%cl, %s\n", op, reg_name(w, 8));
	}
	x86_store_reg(f, w, i->dst);
}

void x86_make_unary (X86Fn *f, IRInstr *i, const char *op)
{
	uint8_t w = x86_work_reg(f, i->dst, 0);

	x86_load_reg(f, w, i->src1);
	cg_printf(f->cg, "\t%sq %s\n", op, reg_name(w, 8));
	x86_store_reg(f, w, i->dst);
}

void x86_make_not_l (X86Fn *f, IRInstr *i)
{
	uint8_t w = x86_work_reg(f, i->dst, 0);
	const char *q = reg_name(w, 8);
	const char *b = reg_name(w, 1);

	x86_load_reg(f, w, i->src1);
	cg_printf(f->cg, "\ttestq %s, %s\n\tsete %s\n\tmovzbl %s, %s\n",
			q, q, b, b, reg_name(w, 4));
	x86_store_reg(f, w, i->dst);
}

static int mul_tmp (int64_t v)
{
	if (v > 0 && (v & (v - 1)) == 0) return 0;
	if (v == 3 || v == 5 || v == 9) return 0;
	if (v > 9 && !((uint64_t)(v - 1) & (uint64_t)(v - 2))) return 1;
	if (v > 3 && !((uint64_t)v & ((uint64_t)v + 1))) return 1;
	return v != (int32_t)v;
}

uint32_t x86_arith_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	if (x86_rmw_fits(f, i, h[SC_SRC2])) return 0;

	int8_t d = h[SC_DST];
	if (i->op == IR_MUL && f->regs[i->src2].cstate == VR_CONST)
		return (d < 0) + mul_tmp(x86_const_ext(f, i->src2));

	if (i->op != IR_SUB && d >= 0 && d == h[SC_SRC2])
		return x86_op_reg_tmp(f, i->src1, h[SC_SRC1]);

	uint32_t w = d < 0 || d == h[SC_SRC2];
	return w + x86_op_reg_tmp(f, i->src2, h[SC_SRC2]);
}

uint32_t x86_unary_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	if (x86_rmw_fits(f, i, h[SC_SRC2])) return 0;
	return h[SC_DST] < 0;
}
