#include "codegen/x86-64/common.h"

static IRInstr *group_call (IRInstr *arg, IRInstr *end)
{
	while (arg < end && arg->op == IR_ARG) arg++;
	return arg;
}

static void move_call_live (X86Fn *f, IRInstr *call, int restore)
{
	uint32_t at = (uint32_t)(call - (f->cg->ir->instrs + f->fn->start));
	uint64_t keep = f->ctx.save[at] & ~X86_CALLEE_MASK;

	x86_move_saved(f, keep, restore);
}

void x86_make_arg (X86Fn *f, IRInstr *i)
{
	int is_const = f->regs[i->src1].cstate == VR_CONST;
	int64_t v = is_const ? x86_const_ext(f, i->src1) : 0;
	int p = x86_phys_reg(f, i->src1);

	if (i->target == 0) {
		IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
		IRInstr *call = group_call(i, end);

		x86_check_arg_group(f, i, end);
		if (!x86_tail_match(f, call, x86_tail_ret(call)))
			move_call_live(f, call, 0);
	}
	if (i->target < 6) {
		x86_take_fixed(f, x86_arg_regs[i->target], i->src1);
		x86_load_reg(f, x86_arg_regs[i->target], i->src1);
		return;
	}
	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\tmovq $%lld, %u(%%rsp)\n",
				(long long)v, (i->target - 6) << 3);
		return;
	}

	uint8_t w = 0;
	f->busy |= ARG_REGS_MASK;
	w = p >= 0 ? (uint8_t)p : x86_grab(f);
	if (p < 0) x86_load_reg(f, w, i->src1);
	cg_printf(f->cg, "\tmovq %s, %u(%%rsp)\n", reg_name(w, 8), (i->target - 6) << 3);
}

void x86_make_call (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;

	if (i->argc == 0) move_call_live(f, i, 0);
	cg_printf(c, "\tcall ");
	x86_print_fn_name(c, c->ir->fns + i->target);
	cg_printf(c, "\n");

	if (i->dst != NO_REG)
		x86_store_reg(f, RAX, i->dst);
	move_call_live(f, i, 1);
}

void x86_make_ret (X86Fn *f, IRInstr *i)
{
	if (i->src1 != NO_REG) {
		x86_take_fixed(f, RAX, i->src1);
		x86_load_reg(f, RAX, i->src1);
	}
	x86_move_saved(f, f->ctx.callee_used, 1);
	cg_printf(f->cg, f->leaf ? "\tret\n" : "\tleave\n\tret\n");
}

IRInstr *x86_tail_ret (IRInstr *call)
{
	IRInstr *n = call + 1;

	while (n->op == IR_LABEL) n++;
	return n;
}

int x86_tail_match (X86Fn *f, IRInstr *call, IRInstr *ret)
{
	uint8_t from = f->cg->ir->fns[call->target].ret_type;
	uint8_t to = f->fn->ret_type;

	if (ret->op != IR_RET || call->argc > 6) return 0;
	if (ret->src1 != call->dst) return 0;
	if (call->dst != NO_REG && f->regs[call->dst].uses != 1) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

int x86_make_tail_call (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRInstr *ret = x86_tail_ret(i);
	if (!x86_tail_match(f, i, ret)) return 0;

	if (c->ir->fns + i->target == f->fn) {
		cg_printf(c, "\tjmp .LS%u\n", i->target);
	} else {
		x86_move_saved(f, f->ctx.callee_used, 1);
		if (!f->leaf) cg_printf(c, "\tleave\n");
		cg_printf(c, "\tjmp ");
		x86_print_fn_name(c, c->ir->fns + i->target);
		cg_printf(c, "\n");
	}

	return ret == i + 1 ? 2 : 1;
}

