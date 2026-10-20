#include "codegen/x86-64/common.h"

static const char *comp_names[12] = {
	"e", "ne", "g", "ge", "l", "le",
	"e", "ne", "a", "ae", "b", "be"
};

static int cmp_mem_imm (X86Fn *f, IRInstr *i)
{
	uint32_t a = i->src1;
	uint8_t s = types[f->fn->reg_types[a]].size;

	if (f->regs[i->src2].cstate != VR_CONST) return 0;
	if (f->regs[a].cstate != VR_MEM && f->regs[a].cstate != VR_ALIAS) return 0;
	if (x86_phys_reg(f, a) >= 0) return 0;

	int64_t v = x86_const_ext(f, i->src2);
	if (s == 8 && v != (int32_t)v) return 0;
	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;

	cg_printf(f->cg, "\tcmp%c $%lld, -%u(%s)\n", x86_mem_suf[s],
			(long long)v, f->regs[a].slot, f->base);
	return 1;
}

static void write_cmp (X86Fn *f, IRInstr *i)
{
	uint64_t keep = f->pool;
	if (cmp_mem_imm(f, i)) return;

	int p = x86_phys_reg(f, i->src1);
	if (p < 0) {
		p = x86_grab(f);
		x86_load_reg(f, (uint8_t)p, i->src1);
	}
	x86_op_reg(f, "cmp", (uint8_t)p, i->src2);
	f->pool = keep;
}

static void make_cmp (X86Fn *f, IRInstr *i)
{
	int base = types[i->data_type].sign ? 0 : 6;
	const char *cc = comp_names[base + (i->op - IR_EQ)];

	write_cmp(f, i);
	uint8_t w = x86_work_reg(f, i->dst, 0);
	cg_printf(f->cg, "\tset%s %s\n\tmovzbl %s, %s\n",
			cc, reg_name(w, 1), reg_name(w, 1), reg_name(w, 4));
	x86_store_reg(f, w, i->dst);
}

static const uint8_t inv_cmp[6] = { 1, 0, 5, 4, 3, 2 };

static int fuses_with_next (X86Fn *f, IRInstr *i)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *n = x86_next_live(f, i, end);

	if (n == end) return 0;
	if (n->op != IR_JZ && n->op != IR_JNZ) return 0;
	return n->src1 == i->dst && f->regs[i->dst].uses == 1;
}

static void make_cmp_jump (X86Fn *f, IRInstr *cmp, IRInstr *jmp)
{
	int base = types[cmp->data_type].sign ? 0 : 6;
	int cc = cmp->op - IR_EQ;

	if (jmp->op == IR_JZ) cc = inv_cmp[cc];
	write_cmp(f, cmp);
	cg_printf(f->cg, "\tj%s .L%u\n", comp_names[base + cc], jmp->target);
}

static void select_move (X86Fn *f, IRInstr *sel, const char *yes, const char *no)
{
	int pa = x86_phys_reg(f, sel->src1);
	int pb = x86_phys_reg(f, sel->src2);
	int pd = x86_phys_reg(f, sel->dst);

	if (pd >= 0 && pd == pb) {
		uint8_t r = pa >= 0 ? (uint8_t)pa : x86_grab(f);
		if (pa < 0) x86_load_reg(f, r, sel->src1);
		cg_printf(f->cg, "\tcmov%s %s, %s\n", yes, reg_name(r, 8), reg_name(pd, 8));
		return;
	}
	uint8_t w = x86_work_reg(f, sel->dst, 0);
	x86_load_reg(f, w, sel->src1);

	uint8_t r = pb >= 0 ? (uint8_t)pb : x86_grab(f);
	if (pb < 0) x86_load_reg(f, r, sel->src2);
	cg_printf(f->cg, "\tcmov%s %s, %s\n", no, reg_name(r, 8), reg_name(w, 8));
	x86_store_reg(f, w, sel->dst);
}

static void make_cmp_select (X86Fn *f, IRInstr *cmp, IRInstr *sel)
{
	int base = types[cmp->data_type].sign ? 0 : 6;
	int cc = cmp->op - IR_EQ;

	f->busy |= x86_home_bit(f, sel->src1) | x86_home_bit(f, sel->src2) | x86_home_bit(f, sel->dst);
	write_cmp(f, cmp);
	select_move(f, sel, comp_names[base + cc], comp_names[base + inv_cmp[cc]]);
}

int x86_make_cmp_instr (X86Fn *f, IRInstr *i)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *n = x86_next_live(f, i, end);

	if (n != end && fuses_with_next(f, i)) {
		make_cmp_jump(f, i, n);
		return (int)(n - i) + 1;
	}
	if (n != end && n->op == IR_SELECT && n->cond == i->dst
			&& f->regs[i->dst].uses == 1) {
		make_cmp_select(f, i, n);
		return (int)(n - i) + 1;
	}
	make_cmp(f, i);
	return 1;
}

static void test_zero (X86Fn *f, uint32_t vreg)
{
	int p = x86_phys_reg(f, vreg);
	uint8_t s = types[f->fn->reg_types[vreg]].size;
	uint64_t keep = f->pool;

	if (f->regs[vreg].cstate == VR_CONST) {
		uint8_t t = x86_grab(f);
		x86_load_reg(f, t, vreg);
		cg_printf(f->cg, "\ttestq %s, %s\n", reg_name(t, 8), reg_name(t, 8));
		f->pool = keep;
		return;
	}
	if (p >= 0) cg_printf(f->cg, "\ttestq %s, %s\n", reg_name(p, 8), reg_name(p, 8));
	else cg_printf(f->cg, "\tcmp%c $0, -%u(%s)\n", x86_mem_suf[s], f->regs[vreg].slot, f->base);
}

int x86_const_jump (X86Fn *f, IRInstr *i)
{
	if (i->op != IR_JZ && i->op != IR_JNZ) return 0;
	if (f->regs[i->src1].cstate != VR_CONST) return 0;
	return (x86_const_ext(f, i->src1) != 0) == (i->op == IR_JNZ);
}

void x86_make_jump (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;

	if (i->op == IR_LABEL) {
		cg_printf(c, ".L%u:\n", i->target);
		return;
	}
	if (i->op == IR_JMP) {
		cg_printf(c, "\tjmp .L%u\n", i->target);
		return;
	}
	if (f->regs[i->src1].cstate == VR_CONST) {
		if (x86_const_jump(f, i)) cg_printf(c, "\tjmp .L%u\n", i->target);
		return;
	}
	test_zero(f, i->src1);
	cg_printf(c, "\t%s .L%u\n", i->op == IR_JZ ? "je" : "jne", i->target);
}

void x86_make_select (X86Fn *f, IRInstr *i)
{
	test_zero(f, i->cond);
	select_move(f, i, "ne", "e");
}

