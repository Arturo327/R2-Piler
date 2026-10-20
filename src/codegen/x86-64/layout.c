#include "codegen/x86-64/common.h"

int x86_is_silent (X86Fn *f, IRInstr *i)
{
	if (i->op == IR_NOP) return 1;
	if (i->dst == NO_REG) return 0;
	if (f->regs[i->dst].cstate == VR_CONST || f->regs[i->dst].cstate == VR_ALIAS)
		return 1;
	return f->regs[i->dst].uses == 0 && is_deletable[i->op];
}

IRInstr *x86_next_live (X86Fn *f, IRInstr *i, IRInstr *end)
{
	i++;
	while (i < end && x86_is_silent(f, i)) i++;
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

static void fill_regalloc (X86Fn *f)
{
	IRFn *fn = f->fn;
	int spill_all = f->cg->opt_level == NO_OPT;

	f->ctx.ins_block = f->blocks;
	f->ctx.save = f->saves;
	f->ctx.free_regs = f->frees;
	f->ctx.edges = f->edges;
	f->ctx.callee_mask = X86_CALLEE_MASK;
	f->ctx.caller_mask = X86_CALLER_HOME_MASK;
	f->ctx.tail_argc_max = X86_TAIL_ARGC_MAX;
	regalloc(f->cg->arena, f->cg->ir, fn, f->regs, &f->ctx, X86_REG_NUM, spill_all);
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
