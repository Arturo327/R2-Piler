#include "codegen/x86-64/common.h"

static void make_action (X86Fn *f, const PMAction *a)
{
	switch (a->kind)
	{
	case PM_ACT_STORE:
		cg_printf(f->cg, "\tmov%c %s, -%u(%s)\n", x86_mem_suf[a->size],
			reg_name(a->from, a->size), a->slot, f->base);
		return;
	case PM_ACT_LOAD:
		uint8_t w;
		const char *op = x86_load_op_bits(a->size, a->sign, &w);
		cg_printf(f->cg, "\t%s -%u(%s), %s\n", op,
				a->slot, f->base, reg_name(a->to, w));
		return;
	case PM_ACT_SWAP:
		cg_printf(f->cg, "\txchgq %s, %s\n",
				reg_name(a->from, 8), reg_name(a->to, 8));
		return;
	case PM_ACT_MOVE:
		cg_printf(f->cg, "\tmovq %s, %s\n",
				reg_name(a->from, 8), reg_name(a->to, 8));
		return;
	default:
		return;
	}
}

void x86_parallel_move (X86Fn *f, PMove *moves, uint32_t count)
{
	uint32_t n = parallel_actions(moves, count, f->acts);

	for (uint32_t i = 0; i < n; i++)
		make_action(f, f->acts + i);
}

static uint32_t param_group (X86Fn *f, IRInstr *first)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	uint32_t n = 0;

	while (first + n < end && first[n].op == IR_PARAM) n++;
	return n;
}

static uint32_t build_param_moves (X86Fn *f, IRInstr *first, uint32_t n)
{
	uint32_t count = 0;

	for (uint32_t k = 0; k < n; k++) {
		IRInstr *p = first + k;
		Reg *r = f->regs + p->dst;
		int h = r->home[f->blk];

		if (p->target >= 6 || !r->uses || h == VR_NO_HOME) continue;
		f->moves[count].from = (int8_t)x86_arg_regs[p->target];
		f->moves[count].to = h >= 0 ? (int8_t)h : PM_SLOT;
		f->moves[count].slot = r->slot;
		f->moves[count].size = types[f->fn->reg_types[p->dst]].size;
		f->moves[count].sign = types[f->fn->reg_types[p->dst]].sign;
		count++;
	}
	return count;
}

static void extend_home (X86Fn *f, uint8_t reg, TypeId type)
{
	uint8_t size = types[type].size;
	uint8_t w;
	const char *op = x86_load_op_bits(size, types[type].sign, &w);

	if (size == 8) return;
	cg_printf(f->cg, "\t%s %s, %s\n", op, reg_name(reg, size), reg_name(reg, w));
}

static void normalize_params (X86Fn *f, IRInstr *first, uint32_t n)
{
	for (uint32_t k = 0; k < n; k++) {
		IRInstr *p = first + k;
		int h = f->regs[p->dst].home[f->blk];

		if (p->target >= 6 || !f->regs[p->dst].uses || h < 0) continue;
		extend_home(f, (uint8_t)h, f->fn->reg_types[p->dst]);
	}
}

static void make_stack_param (X86Fn *f, IRInstr *p)
{
	uint64_t keep = f->pool;
	uint8_t w = x86_work_reg(f, p->dst, 0);

	cg_printf(f->cg, "\tmovq %u(%s), %s\n",
			(f->leaf ? 8 : 16) + ((p->target - 6) << 3),
			f->base, reg_name(w, 8));
	x86_store_reg(f, w, p->dst);
	f->pool = keep;
}

int x86_make_param (X86Fn *f, IRInstr *i)
{
	uint32_t n = param_group(f, i);
	uint32_t count = build_param_moves(f, i, n);

	for (uint32_t k = 0; k < n; k++)
		f->busy |= x86_home_bit(f, i[k].dst);
	x86_parallel_move(f, f->moves, count);
	normalize_params(f, i, n);
	for (uint32_t k = 0; k < n; k++)
		if (i[k].target >= 6 && f->regs[i[k].dst].uses)
			make_stack_param(f, i + k);
	return (int)n;
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

	if (p < 0 && pd < 0 && f->regs[i->dst].slot != 0 &&
			f->regs[i->dst].slot == f->regs[i->src1].slot)
		return;
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

uint32_t x86_move_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	if (i->dst == i->src1 || h[SC_DST] != VR_SPILL) return 0;
	if (f->regs[i->src1].cstate != VR_CONST)
		return h[SC_SRC1] == VR_SPILL || f->regs[i->src1].cstate == VR_GLOBAL;
	int64_t v = x86_const_ext(f, i->src1);
	return types[f->fn->reg_types[i->dst]].size == 8 && v != (int32_t)v;
}

uint32_t x86_const_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	if (h[SC_DST] != VR_SPILL) return 0;
	return types[f->fn->reg_types[i->dst]].size == 8
		&& i->imm64 != (int32_t)i->imm64;
}
