#include "codegen/x86-64/common.h"

void x86_make_addr_local (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	Reg *src = f->regs + i->src1;
	int p = x86_phys_reg(f, i->dst);
	uint8_t w = p >= 0 ? (uint8_t)p : x86_grab(f);

	x86_clobber(f, w);
	cg_printf(c, "\tleaq %d(%s), %s\n", x86_slot_off(f, src->slot), f->base, reg_name(w, 8));
	if (p < 0) x86_store_reg(f, w, i->dst);
}

void x86_make_addr_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;
	int p = x86_phys_reg(f, i->dst);
	uint8_t w = p >= 0 ? (uint8_t)p : x86_grab(f);

	x86_clobber(f, w);
	cg_printf(c, "\tleaq %s%.*s(%%rip), %s\n",
			GLOBAL_PREFIX(g), (int)g->len, g->name, reg_name(w, 8));
	if (p < 0) x86_store_reg(f, w, i->dst);
}

void x86_make_str_lit (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	int p = x86_phys_reg(f, i->dst);
	uint8_t w = p >= 0 ? (uint8_t)p : x86_grab(f);

	x86_clobber(f, w);
	cg_printf(c, "\tleaq .LC%u(%%rip), %s\n", i->target, reg_name(w, 8));
	if (p < 0) x86_store_reg(f, w, i->dst);
}

static uint8_t ptr_reg (X86Fn *f, uint32_t vreg)
{
	int p = x86_phys_reg(f, vreg);

	if (p >= 0) return (uint8_t)p;
	uint8_t w = x86_grab(f);
	x86_load_reg(f, w, vreg);
	return w;
}

void x86_make_ld_ptr (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t w;
	const char *op = x86_load_op(i->data_type, &w);
	uint8_t base = ptr_reg(f, i->src1);
	int d = x86_phys_reg(f, i->dst);

	if (d >= 0) {
		x86_clobber(f, (uint8_t)d);
		cg_printf(c, "\t%s %d(%s), %s\n", op, (int)i->imm64, reg_name(base, 8), reg_name((uint8_t)d, w));
		return;
	}
	uint8_t val = x86_grab(f);
	cg_printf(c, "\t%s %d(%s), %s\n", op, (int)i->imm64, reg_name(base, 8), reg_name(val, w));
	x86_store_reg(f, val, i->dst);
}

void x86_make_str_ptr (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t size = types[i->data_type].size;
	uint8_t base = ptr_reg(f, i->src1);
	int v = x86_phys_reg(f, i->src2);
	uint8_t val = v >= 0 ? (uint8_t)v : x86_grab(f);

	if (v < 0) x86_load_reg(f, val, i->src2);
	cg_printf(c, "\tmov%c %s, %d(%s)\n", x86_mem_suf[size],
			reg_name(val, size), (int)i->imm64, reg_name(base, 8));
}

void x86_make_ld_local (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t w;
	const char *op = x86_load_op(i->data_type, &w);
	int off = x86_slot_off(f, f->regs[i->src1].slot) + (int)i->imm64;
	int d = x86_phys_reg(f, i->dst);

	if (d >= 0) {
		x86_clobber(f, (uint8_t)d);
		cg_printf(c, "\t%s %d(%s), %s\n", op, off, f->base, reg_name((uint8_t)d, w));
		return;
	}
	uint8_t val = x86_grab(f);
	cg_printf(c, "\t%s %d(%s), %s\n", op, off, f->base, reg_name(val, w));
	x86_store_reg(f, val, i->dst);
}

void x86_make_str_local (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t size = types[i->data_type].size;
	int off = x86_slot_off(f, f->regs[i->src1].slot) + (int)i->imm64;
	int v = x86_phys_reg(f, i->src2);
	uint8_t val = v >= 0 ? (uint8_t)v : x86_grab(f);

	if (v < 0) x86_load_reg(f, val, i->src2);
	cg_printf(c, "\tmov%c %s, %d(%s)\n", x86_mem_suf[size],
			reg_name(val, size), off, f->base);
}

uint32_t x86_ld_ptr_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	(void)f;
	(void)i;
	uint32_t n = h[SC_SRC1] < 0;

	if (h[SC_DST] == VR_SPILL) n++;
	return n;
}

uint32_t x86_str_ptr_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	(void)f;
	(void)i;
	return (uint32_t)(h[SC_SRC1] < 0) + (uint32_t)(h[SC_SRC2] < 0);
}

void x86_make_strings (CodeGen *c)
{
	IR *ir = c->ir;

	if (!ir->string_count) return;
	cg_printf(c, "\t.section .rodata\n");
	for (uint32_t i = 0; i < ir->string_count; i++) {
		IRString *s = ir->strings + i;

		cg_printf(c, ".LC%u:\n", i);
		for (uint32_t j = 0; j < s->len; j += 32) {
			uint32_t n = s->len - j < 32 ? s->len - j : 32;

			cg_printf(c, "\t.byte ");
			for (uint32_t k = 0; k < n; k++) {
				if (k) cg_printf(c, ", ");
				cg_printf(c, "%u", (unsigned)(unsigned char)s->data[j + k]);
			}
			cg_printf(c, "\n");
		}
		cg_printf(c, "\t.byte 0\n");
	}
}
