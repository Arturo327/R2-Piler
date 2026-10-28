#include "codegen/x86-64/common.h"

IRInstr *x86_next_live (X86Fn *f, IRInstr *i, IRInstr *end)
{
	i++;
	while (i < end && cg_is_silent(f->regs, i)) i++;
	return i;
}

static uint32_t scan_outg (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	uint32_t out = 0;

	f->leaf = 1;
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *instr = ir->instrs + fn->start + i;

		if (instr->op != IR_CALL) continue;
		if (x86_tail_match(f, instr)) continue;
		f->leaf = 0;

		if (instr->argc <= 6) continue;
		uint32_t n = (instr->argc - 6) << 3;
		if (n > out) out = n;
	}
	return out;
}

static uint32_t save_area (X86Fn *f, uint32_t c)
{
	uint64_t mask = f->ctx.callee_used;

	for (uint32_t i = 0; i < f->fn->count; i++)
		mask |= f->ctx.save[i];

	f->regsave = 0;
	f->savemask = mask;
	if (!mask) return c;
	f->regsave = (c + 7) & ~7u;
	return f->regsave + 8u * (uint32_t)__builtin_popcountll(mask);
}

static uint32_t x86_scratch_need (void *user, IRInstr *in, const int8_t *h)
{
	X86Fn *f = user;

	switch (in->op)
	{
	case IR_CONST: return x86_const_scratch(f, in, h);
	case IR_PARAM: return in->target >= X86_ARG_REG_MAX && h[SC_DST] == VR_SPILL;
	case IR_LD_GLOBAL: return h[SC_DST] == VR_SPILL;
	case IR_STR_GLOBAL: return x86_str_global_scratch(f, in, h);
	case IR_MOVE: case IR_EXTEND: return x86_move_scratch(f, in, h);
	case IR_SELECT: return x86_select_scratch(f, in, h);
	case IR_ADD: case IR_SUB: case IR_MUL: case IR_AND_A: case IR_OR_A: case IR_XOR:
		return x86_arith_scratch(f, in, h);
	case IR_DIV: case IR_MOD: return x86_divmod_scratch(f, in, h);
	case IR_RS: case IR_LS: case IR_NEG: case IR_NOT_A: case IR_NOT_L:
		return x86_unary_scratch(f, in, h);
	case IR_EQ: case IR_NE: case IR_GT: case IR_GE: case IR_LT: case IR_LE:
		return x86_cmp_scratch(f, in, h);
	case IR_ARG: return x86_arg_scratch(f, in, h);
	default: return 0;
	}
}

static void fill_regalloc (X86Fn *f)
{
	IRFn *fn = f->fn;
	int spill_all = f->cg->opt_level == NO_OPT;

	f->ctx.ins_block = f->blocks;
	f->ctx.save = f->saves;
	f->ctx.free_regs = f->frees;
	f->ctx.edges = f->edges;
	f->ctx.callee_mask = X86_CALLEE_MASK;
	f->ctx.caller_mask = X86_CALLER_MASK;
	f->ctx.arg_reg_max = X86_ARG_REG_MAX;
	f->ctx.arg_regs = x86_arg_regs;
	f->ctx.scratch_max = X86_SCRATCH_MAX;
	f->ctx.scratch_need = x86_scratch_need;
	f->ctx.user = f;

	regalloc(f->cg->arena, f->cg->ir, fn, f->regs, &f->ctx, X86_REG_NUM, x86_64_needs, spill_all);
}

static uint32_t max_slot (X86Fn *f)
{
	uint32_t m = 0;

	for (uint32_t r = 0; r < f->fn->reg_count; r++)
		if (f->regs[r].slot != 0 && f->regs[r].slot > m)
			m = f->regs[r].slot;
	return m;
}

void x86_layout_fn (X86Fn *f)
{
	fill_regalloc(f);

	uint32_t c = save_area(f, max_slot(f));
	f->outgoing = scan_outg(f);
	c += f->outgoing;

	f->leaf = f->leaf && c <= RED_ZONE;
	f->frame = f->leaf ? 0 : (c + 15) & ~15u;
	f->base = f->leaf ? "%rsp" : "%rbp";
}
