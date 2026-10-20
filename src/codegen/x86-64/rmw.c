#include "codegen/x86-64/common.h"

enum {
	RMW_NONE = 0,
	RMW_UNARY,
	RMW_ARITH,
	RMW_SHIFT
};

static const uint8_t rmw_kind[IR_COUNT] = {
	[IR_ADD] = RMW_ARITH, [IR_SUB] = RMW_ARITH, [IR_AND_A] = RMW_ARITH,
	[IR_OR_A] = RMW_ARITH, [IR_XOR] = RMW_ARITH,
	[IR_NEG] = RMW_UNARY, [IR_NOT_A] = RMW_UNARY,
	[IR_MUL] = RMW_SHIFT, [IR_LS] = RMW_SHIFT, [IR_RS] = RMW_SHIFT
};

static const char *const rmw_names[IR_COUNT] = {
	[IR_ADD] = "add", [IR_SUB] = "sub", [IR_AND_A] = "and",
	[IR_OR_A] = "or", [IR_XOR] = "xor", [IR_NEG] = "neg", [IR_NOT_A] = "not"
};

static int rmw_shift (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	if (f->regs[b->src2].cstate != VR_CONST) return 0;
	int64_t k = x86_const_ext(f, b->src2);
	if (b->op == IR_MUL) {
		if (k <= 1 || (k & (k - 1))) return 0;
		k = (int64_t)x86_shift_amt((uint64_t)k);
	}
	if (k < 1 || k >= 8 * s) return 0;

	if (b->op == IR_RS) *op = types[b->data_type].sign ? "sar" : "shr";
	else *op = "shl";
	*v = k;
	return 1;
}

static int rmw_arith (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	if (f->regs[b->src2].cstate != VR_CONST) return 0;
	int64_t k = x86_const_ext(f, b->src2);
	if (s == 8 && k != (int32_t)k) return 0;

	if (s == 4) k = (int32_t)k;
	else if (s == 2) k = (int16_t)k;
	else if (s == 1) k = (int8_t)k;

	*op = rmw_names[b->op];
	*v = k;
	return 1;
}

static int rmw_operand (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	switch (rmw_kind[b->op])
	{
	case RMW_UNARY: *op = rmw_names[b->op]; return 1;
	case RMW_ARITH: return rmw_arith(f, b, s, op, v);
	case RMW_SHIFT: return rmw_shift(f, b, s, op, v);
	default: return 0;
	}
}

static int rmw_reg_op (X86Fn *f, IRInstr *b, uint8_t s, const char **op)
{
	uint32_t y = b->src2;

	if (rmw_kind[b->op] != RMW_ARITH) return 0;
	if (y == NO_REG) return 0;
	if (x86_phys_reg(f, y) < 0) return 0;

	if (types[f->fn->reg_types[y]].size != s) return 0;
	*op = rmw_names[b->op];
	return 1;
}

static int rmw_closes (IRInstr *a, IRInstr *c)
{
	if (a->op == IR_LD_GLOBAL)
		return c->op == IR_STR_GLOBAL && c->target == a->target;
	return c->op == IR_MOVE && c->dst == a->src1;
}

static IRInstr *rmw_find (X86Fn *f, IRInstr *a, IRInstr **mid)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *b = x86_next_live(f, a, end);

	if (b == end || !rmw_kind[b->op] || b->src1 != a->dst) return NULL;
	IRInstr *c = x86_next_live(f, b, end);
	if (c == end || c->src1 != b->dst || !rmw_closes(a, c)) return NULL;

	*mid = b;
	return c;
}

static uint8_t rmw_width (X86Fn *f, IRInstr *a)
{
	if (a->op == IR_LD_GLOBAL)
		return types[f->cg->ir->globals[a->target].type].size;
	return types[f->fn->reg_types[a->src1]].size;
}

static int rmw_shape_ok (X86Fn *f, IRInstr *a, IRInstr *b)
{
	uint8_t *rt = f->fn->reg_types;
	uint8_t s = rmw_width(f, a);
	Reg *r = f->regs + a->src1;

	if (f->regs[a->dst].uses != 1 || f->regs[b->dst].uses != 1) return 0;
	if (a->op != IR_LD_GLOBAL) {
		int8_t h = r->home[f->blk];

		if (r->cstate != VR_MEM || (h != VR_SPILL && h < 0)) return 0;
	}
	return types[rt[a->dst]].size == s && types[rt[b->dst]].size == s;
}

static const char *const rmw_ext_ops[8] = {
	[1] = "movsbq", [2] = "movswq", [4] = "movslq"
};

static void rmw_sext (X86Fn *f, IRInstr *a)
{
	if (a->op == IR_LD_GLOBAL) return;
	int8_t h = f->regs[a->src1].home[f->blk];
	if (h < 0) return;

	uint8_t t = f->fn->reg_types[a->src1];
	uint8_t s = types[t].size;
	if (s == 8 || !types[t].sign) return;

	uint8_t r = (uint8_t)h;
	cg_printf(f->cg, "\t%s %s, %s\n", rmw_ext_ops[s], reg_name(r, s),
			reg_name(r, 8));
}

static void print_rmw_loc (X86Fn *f, IRInstr *a, uint8_t s)
{
	if (a->op != IR_LD_GLOBAL) {
		int h = f->regs[a->src1].home[f->blk];
		if (h >= 0) {
			cg_printf(f->cg, "%s\n", reg_name((uint8_t)h, s));
			return;
		}
		cg_printf(f->cg, "-%u(%s)\n", f->regs[a->src1].slot, f->base);
		return;
	}
	IRGlobal *g = f->cg->ir->globals + a->target;
	cg_printf(f->cg, SYM_PREFIX "%.*s(%%rip)\n", (int)g->len, g->name);
}

int x86_make_rmw (X86Fn *f, IRInstr *a)
{
	IRInstr *b = NULL;
	IRInstr *c = rmw_find(f, a, &b);
	const char *op = NULL;
	int64_t v = 0;

	if (!c || !rmw_shape_ok(f, a, b)) return 0;
	uint8_t s = rmw_width(f, a);

	if (rmw_operand(f, b, s, &op, &v)) {
		cg_printf(f->cg, "\t%s%c ", op, x86_mem_suf[s]);
		if (rmw_kind[b->op] != RMW_UNARY)
			cg_printf(f->cg, "$%lld, ", (long long)v);
		print_rmw_loc(f, a, s);
		rmw_sext(f, a);
		return (int)(c - a) + 1;
	}

	if (!rmw_reg_op(f, b, s, &op)) return 0;
	uint8_t r = (uint8_t)x86_phys_reg(f, b->src2);
	cg_printf(f->cg, "\t%s%c %s, ", op, x86_mem_suf[s], reg_name(r, s));
	print_rmw_loc(f, a, s);
	rmw_sext(f, a);

	return (int)(c - a) + 1;
}

int x86_try_rmw_inplace (X86Fn *f, IRInstr *i)
{
	if (!rmw_kind[i->op] || i->dst != i->src1) return 0;

	uint32_t x = i->dst;
	int8_t h = f->regs[x].home[f->blk];
	if (f->regs[x].cstate != VR_MEM || (h != VR_SPILL && h < 0)) return 0;
	uint8_t s = types[f->fn->reg_types[x]].size;

	int64_t v = 0;
	const char *op = NULL;
	if (rmw_operand(f, i, s, &op, &v)) {
		cg_printf(f->cg, "\t%s%c ", op, x86_mem_suf[s]);
		if (rmw_kind[i->op] != RMW_UNARY)
			cg_printf(f->cg, "$%lld, ", (long long)v);
		print_rmw_loc(f, i, s);
		rmw_sext(f, i);
		return 1;
	}

	if (!rmw_reg_op(f, i, s, &op)) return 0;
	uint8_t r = (uint8_t)x86_phys_reg(f, i->src2);
	cg_printf(f->cg, "\t%s%c %s, ", op, x86_mem_suf[s], reg_name(r, s));
	print_rmw_loc(f, i, s);
	rmw_sext(f, i);

	return 1;
}

