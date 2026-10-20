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

static void protect_args (X86Fn *f, IRInstr *first, uint32_t n)
{
	f->busy |= ARG_REGS_MASK;
	for (uint32_t k = 0; k < n; k++)
		f->busy |= x86_home_bit(f, first[k].src1);
}

static void make_stack_arg (X86Fn *f, IRInstr *a)
{
	uint32_t off = (a->target - 6) << 3;
	int is_const = f->regs[a->src1].cstate == VR_CONST;
	int64_t v = is_const ? x86_const_ext(f, a->src1) : 0;
	int p = x86_phys_reg(f, a->src1);
	uint8_t w;

	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\tmovq $%lld, %u(%%rsp)\n", (long long)v, off);
		return;
	}
	w = p >= 0 ? (uint8_t)p : x86_grab(f);
	if (p < 0) x86_load_reg(f, w, a->src1);
	cg_printf(f->cg, "\tmovq %s, %u(%%rsp)\n", reg_name(w, 8), off);
}

static uint32_t build_arg_moves (X86Fn *f, IRInstr *first, uint32_t n)
{
	uint32_t count = 0;

	for (uint32_t k = 0; k < n && k < 6; k++) {
		IRInstr *a = first + k;
		uint32_t src = a->src1;
		int p = x86_phys_reg(f, src);

		if (f->regs[src].cstate == VR_CONST) continue;
		f->moves[count].from = p >= 0 ? (int8_t)p : PM_SLOT;
		f->moves[count].to = (int8_t)x86_arg_regs[k];
		f->moves[count].slot = f->regs[src].slot;
		f->moves[count].size = types[f->fn->reg_types[src]].size;
		f->moves[count].sign = types[f->fn->reg_types[src]].sign;
		count++;
	}
	return count;
}

static void remat_args (X86Fn *f, IRInstr *first, uint32_t n)
{
	for (uint32_t k = 0; k < n && k < 6; k++) {
		IRInstr *a = first + k;

		if (f->regs[a->src1].cstate != VR_CONST) continue;
		x86_load_imm(f, x86_arg_regs[k], x86_const_ext(f, a->src1));
	}
}

int x86_make_arg (X86Fn *f, IRInstr *i)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *call = group_call(i, end);
	uint32_t n = (uint32_t)(call - i);
	uint32_t blk = f->blk;
	uint32_t count;

	if (i->target != 0) return 1;
	x86_check_arg_group(f, i, end);
	f->blk = blk;
	if (!x86_tail_match(f, call, x86_tail_ret(call)))
		move_call_live(f, call, 0);
	protect_args(f, i, n);
	for (uint32_t k = 6; k < n; k++)
		make_stack_arg(f, i + k);
	count = build_arg_moves(f, i, n);
	x86_parallel_move(f, f->moves, count);
	remat_args(f, i, n);
	return (int)n;
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
	uint32_t at = (uint32_t)(call - (f->cg->ir->instrs + f->fn->start));

	if (ret->op != IR_RET || call->argc > 6) return 0;
	if (ret->src1 != call->dst) return 0;
	if (call->dst != NO_REG && f->regs[call->dst].uses != 1) return 0;
	if (f->ctx.edges[at].fall) return 0;
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
