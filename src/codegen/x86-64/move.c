#include "codegen/x86-64/common.h"

void x86_make_param (X86Fn *f, IRInstr *i)
{
	if (!f->regs[i->dst].uses) return;
	if (i->target < 6) {
		x86_store_reg(f, x86_arg_regs[i->target], i->dst);
		return;
	}
	uint8_t w = x86_work_reg(f, i->dst, 0);
	cg_printf(f->cg, "\tmovq %u(%s), %s\n",
			(f->leaf ? 8 : 16) + ((i->target - 6) << 3), f->base, reg_name(w, 8));
	x86_store_reg(f, w, i->dst);
}

void x86_make_move (X86Fn *f, IRInstr *i)
{
	if (i->dst == i->src1) return;
	if (f->regs[i->src1].cstate == VR_CONST) {
		x86_store_imm(f, i->dst, x86_const_ext(f, i->src1));
		return;
	}
	int p = x86_phys_reg(f, i->src1);
	int pd = x86_phys_reg(f, i->dst);

	if (p >= 0 && pd >= 0) {
		if (!f->regs[i->dst].uses) return;
		if (same_rep(f->fn, i->dst, i->src1)) {
			if (p == pd) return;
			cg_printf(f->cg, "\tmovq %s, %s\n",
					reg_name((uint8_t)p, 8), reg_name((uint8_t)pd, 8));
			return;
		}
		x86_store_reg(f, (uint8_t)p, i->dst);
		return;
	}
	if (p >= 0) {
		x86_store_reg(f, (uint8_t)p, i->dst);
		return;
	}
	uint8_t w = x86_work_reg(f, i->dst, 0);
	x86_load_reg(f, w, i->src1);
	x86_store_reg(f, w, i->dst);
}

