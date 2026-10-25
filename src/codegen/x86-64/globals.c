#include "codegen/x86-64/common.h"

static const char *asm_size_name[9] =
{
	[1] = ".byte",
	[2] = ".word",
	[4] = ".long",
	[8] = ".quad"
};

static void make_global_data (CodeGen *c, uint32_t i)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;

	cg_printf(c, "\t.balign %u\n" SYM_PREFIX "%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, "%s ", asm_size_name[size]);

	if (types[g->type].sign) cg_printf(c, "%lld\n", (long long)g->init);
	else cg_printf(c, "%llu\n", (unsigned long long)g->init);
}

static void make_global_bss (CodeGen *c, uint32_t i)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;
	cg_printf(c, "\t.balign %u\n" SYM_PREFIX "%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, ".zero %u\n", size);
}

void x86_make_globals (CodeGen *c)
{
	IR *ir = c->ir;
	int printed = 0;

	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (!ir->globals[i].has_init) continue;
		if (!printed) {
			printed = 1;
			cg_printf(c, "\t.section .data\n");
		}
		make_global_data(c, i);
	}
	printed = 0;
	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (ir->globals[i].has_init) continue;
		if (!printed) {
			printed = 1;
			cg_printf(c, "\t.section .bss\n");
		}
		make_global_bss(c, i);
	}
}

void x86_make_ld_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;
	int p = x86_phys_reg(f, i->dst);
	uint8_t w = p >= 0 ? (uint8_t)p : x86_grab(f);
	uint8_t size;
	const char *op = x86_load_op(g->type, &size);

	cg_printf(c, "\t%s " SYM_PREFIX "%.*s(%%rip), %s\n", op,
			(int)g->len, g->name, reg_name(w, size));
	if (p < 0) x86_store_reg(f, w, i->dst);
}

void x86_make_str_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;
	uint8_t s = types[g->type].size;
	int p = x86_phys_reg(f, i->src1);

	if (f->regs[i->src1].cstate == VR_CONST) {
		int64_t v = x86_const_ext(f, i->src1);

		if (s == 8 && v != (int32_t)v) {
			uint8_t w = x86_grab(f);
			cg_printf(c, "\tmovabsq $0x%llx, %s\n",
					(unsigned long long)v, reg_name(w, 8));
			cg_printf(c, "\tmovq %s, " SYM_PREFIX "%.*s(%%rip)\n",
					reg_name(w, 8), (int)g->len, g->name);
			return;
		}
		if (s == 4) v = (int32_t)v;
		else if (s == 2) v = (int16_t)v;
		else if (s == 1) v = (int8_t)v;
		cg_printf(c, "\tmov%c $%lld, " SYM_PREFIX "%.*s(%%rip)\n", x86_mem_suf[s],
				(long long)v, (int)g->len, g->name);
		return;
	}
	if (p < 0) {
		p = x86_grab(f);
		x86_load_reg(f, (uint8_t)p, i->src1);
	}
	cg_printf(c, "\tmov%c %s, " SYM_PREFIX "%.*s(%%rip)\n", x86_mem_suf[s],
			reg_name(p, s), (int)g->len, g->name);
}

uint32_t x86_str_global_scratch (X86Fn *f, IRInstr *i, const int8_t *h)
{
	if (f->regs[i->src1].cstate != VR_CONST) return h[SC_SRC1] == VR_SPILL;
	int64_t v = x86_const_ext(f, i->src1);
	return types[f->cg->ir->globals[i->target].type].size == 8 && v != (int32_t)v;
}
