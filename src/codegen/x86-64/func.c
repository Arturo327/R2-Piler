#include "codegen/x86-64/common.h"

void x86_print_fn_name (CodeGen *c, IRFn *fn)
{
	if (fn == c->ir->fns + c->ir->init_fn) cg_printf(c, INIT_SYM);
	else cg_printf(c, SYM_PREFIX "%.*s", (int)fn->len, fn->name);
}

static int make_instr (X86Fn *f, IRInstr *i)
{
	if (cg_is_silent(f->regs, i)) return 1;
	x86_begin_instr(f, i);
	if (x86_try_rmw_inplace(f, i)) return 1;
	int n;

	switch (i->op)
	{
	case IR_CONST:
		if (f->regs[i->dst].cstate != VR_CONST) x86_store_imm(f, i->dst, i->imm64);
		return 1;
	case IR_PARAM: return x86_make_param(f, i);

	case IR_LD_GLOBAL:
		n = x86_make_rmw(f, i);
		if (n) return n;
		x86_make_ld_global(f, i);
		return 1;
	case IR_STR_GLOBAL: x86_make_str_global(f, i); return 1;

	case IR_MOVE:
		n = x86_make_rmw(f, i);
		if (n) return n;
		x86_make_move(f, i);
		return 1;
	case IR_EXTEND: x86_make_move(f, i); return 1;

	case IR_ADD: case IR_SUB: case IR_AND_A: case IR_OR_A: case IR_MUL: case IR_XOR:
		x86_make_arith(f, i); return 1;
	case IR_DIV: case IR_MOD: x86_make_divmod(f, i); return 1;
	case IR_RS: case IR_LS: x86_make_shift(f, i); return 1;

	case IR_NEG: x86_make_unary(f, i, "neg"); return 1;
	case IR_NOT_A: x86_make_unary(f, i, "not"); return 1;
	case IR_NOT_L: x86_make_not_l(f, i); return 1;

	case IR_SELECT: x86_make_select(f, i); return 1;
	case IR_EQ: case IR_NE: case IR_GT: case IR_GE: case IR_LT: case IR_LE:
		return x86_make_cmp_instr(f, i);

	case IR_LABEL: case IR_JMP: case IR_JZ: case IR_JNZ: x86_make_jump(f, i); return 1;

	case IR_ARG: return x86_make_arg(f, i);
	case IR_CALL:
		n = x86_make_tail_call(f, i);
		if (n) return n;
		x86_make_call(f, i);
		return 1;
	case IR_RET: x86_make_ret(f, i); return 1;

	default: return 1;
	}
}

static void make_body (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	IRInstr *instr, *last;
	uint32_t i = 0;
	int dead = 0;

	while (i < fn->count) {
		instr = ir->instrs + fn->start + i;
		if (instr->op == IR_LABEL) dead = 0;
		if (dead) {
			i++;
			continue;
		}
		uint32_t n = make_instr(f, instr);
		last = instr + n - 1;
		dead = last->op == IR_RET || last->op == IR_JMP
				|| x86_const_jump(f, last);
		if (!dead) x86_make_edge(f, i + n - 1, 0);
		i += n;
	}
}

void x86_make_fn (X86Fn *f)
{
	CodeGen *c = f->cg;
	IRFn *fn = f->fn;

	cg_printf(c, "\t.p2align 4\n\t.globl ");
	x86_print_fn_name(c, fn);
	cg_printf(c, "\n\t.type ");
	x86_print_fn_name(c, fn);
	cg_printf(c, ", @function\n");
	x86_print_fn_name(c, fn);

	cg_printf(c, ":\n");
	if (!f->leaf) cg_printf(c, "\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (f->frame) cg_printf(c, "\tsubq $%u, %%rsp\n", f->frame);
	x86_move_saved(f, f->ctx.callee_used, 0);
	cg_printf(c, ".LS%u:\n", (uint32_t)(fn - c->ir->fns));

	make_body(f);
	x86_make_stubs(f);

	cg_printf(c, "\t.size ");
	x86_print_fn_name(c, fn);
	cg_printf(c, ", .-");
	x86_print_fn_name(c, fn);
	cg_printf(c, "\n");
}
