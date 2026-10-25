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

static uint32_t scratch_cmp (IRFn *fn, Reg *regs, IRInstr *in, uint32_t mem)
{
	int imm2 = regs[in->src2].cstate == VR_CONST;
	int narrow2 = types[fn->reg_types[in->src2]].size < 8;
	uint32_t load = ((mem & MEM_SRC1) && !imm2) + ((mem & MEM_SRC2) && narrow2);
	uint32_t store = (mem & MEM_DST) != 0;

	return load > store ? load : store;
}

static uint32_t x86_scratch_need (IRFn *fn, Reg *regs, IRInstr *in, uint32_t mem)
{
	uint32_t m_dst = (mem & MEM_DST) != 0;
	uint32_t m_src1 = (mem & MEM_SRC1) != 0;
	uint32_t m_src2 = (mem & MEM_SRC2) != 0;
	uint32_t sel;

	switch (in->op)
	{
	case IR_ADD: case IR_SUB: case IR_MUL: case IR_AND_A: case IR_OR_A: case IR_XOR:
		return m_dst + (m_src2 && types[fn->reg_types[in->src2]].size < 8);
	case IR_DIV: case IR_MOD:
		return m_src2;
	case IR_RS: case IR_LS: case IR_NEG: case IR_NOT_A: case IR_NOT_L: case IR_LD_GLOBAL:
		return m_dst;
	case IR_STR_GLOBAL:
		return m_src1;
	case IR_MOVE: case IR_EXTEND:
		return m_dst && m_src1;
	case IR_PARAM:
		return m_dst && in->target >= X86_ARG_REG_MAX;
	case IR_ARG:
		return m_src1 && in->target >= X86_ARG_REG_MAX;
	case IR_EQ: case IR_NE: case IR_GT: case IR_GE: case IR_LT: case IR_LE:
		return scratch_cmp(fn, regs, in, mem);
	case IR_SELECT:
		sel = m_dst + m_src2;
		return sel ? sel : m_src1;
	default:
		return 0;
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
	f->ctx.scratch_max = X86_SCRATCH_MAX;
	f->ctx.scratch_need = x86_scratch_need;

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
