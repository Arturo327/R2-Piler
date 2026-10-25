#include "codegen/x86-64/common.h"

const char *x86_regs_names[] =
{
	// 1 byte
	"%al", "%cl", "%dl", "%sil",
	"%dil", "%r8b", "%r9b", "%r10b",
	"%r11b", "%bl", "%r12b", "%r13b",
	"%r14b", "%r15b", NULL, NULL,

	// 2 bytes
	"%ax", "%cx", "%dx", "%si",
	"%di", "%r8w", "%r9w", "%r10w",
	"%r11w", "%bx", "%r12w", "%r13w",
	"%r14w", "%r15w", NULL, NULL,

	// 4 bytes
	"%eax", "%ecx", "%edx", "%esi",
	"%edi", "%r8d", "%r9d", "%r10d",
	"%r11d", "%ebx", "%r12d", "%r13d",
	"%r14d", "%r15d", NULL, NULL,

	// 8 bytes
	"%rax", "%rcx", "%rdx", "%rsi",
	"%rdi", "%r8", "%r9", "%r10",
	"%r11", "%rbx", "%r12", "%r13",
	"%r14", "%r15", NULL, NULL
};

const char x86_mem_suf[9] = { [1] = 'b', [2] = 'w', [4] = 'l', [8] = 'q' };
const uint8_t x86_arg_regs[6] = { 4, 3, 2, 1, 5, 6 };
static const uint8_t size_idx[9] = { [1] = 0, [2] = 1, [4] = 2, [8] = 3 };
static const char *const load_ops[8] = {
	"movzbl", "movsbq", "movzwl", "movswq",
	"movl", "movslq", "movq", "movq"
};

const char *x86_load_op_bits (uint8_t size, uint8_t sign, uint8_t *w)
{
	*w = (size < 8 && !sign) ? 4 : 8;
	return load_ops[(size_idx[size] << 1) | sign];
}

const char *x86_load_op (uint8_t t, uint8_t *size)
{
	const Type *ty = types + t;
	return x86_load_op_bits(ty->size, ty->sign, size);
}

int64_t x86_const_ext (X86Fn *f, uint32_t vreg)
{
	const Type *t = types + f->fn->reg_types[vreg];
	unsigned sh = 64u - t->size * 8u;
	uint64_t v = (uint64_t)f->regs[vreg].cval << sh;

	return t->sign ? (int64_t)v >> sh : (int64_t)(v >> sh);
}

void x86_load_imm (X86Fn *f, uint8_t reg, int64_t v)
{
	CodeGen *c = f->cg;

	if (v >= 0 && v <= UINT32_MAX)
		cg_printf(c, "\tmovl $%lld, %s\n", (long long)v, reg_name(reg, 4));
	else if (v == (int32_t)v)
		cg_printf(c, "\tmovq $%lld, %s\n", (long long)v, reg_name(reg, 8));
	else cg_printf(c, "\tmovabsq $0x%llx, %s\n",
			(unsigned long long)v, reg_name(reg, 8));
}

void x86_load_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	int p = x86_phys_reg(f, vreg);

	if (f->regs[vreg].cstate == VR_CONST) {
		x86_load_imm(f, reg, x86_const_ext(f, vreg));
		return;
	}
	if (p >= 0) {
		if (p != reg)
			cg_printf(f->cg, "\tmovq %s, %s\n", reg_name(p, 8), reg_name(reg, 8));
		return;
	}

	uint8_t w;
	const char *op = x86_load_op(f->fn->reg_types[vreg], &w);
	cg_printf(f->cg, "\t%s -%u(%s), %s\n",
			op, f->regs[vreg].slot, f->base, reg_name(reg, w));
}

static void store_phys (X86Fn *f, uint8_t from, uint8_t to, uint8_t type)
{
	uint8_t s = types[type].size;
	if (from == to && s == 8) return;

	uint8_t w;
	const char *op = x86_load_op(type, &w);
	cg_printf(f->cg, "\t%s %s, %s\n", op, reg_name(from, s), reg_name(to, w));
}

void x86_store_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	uint8_t t = f->fn->reg_types[vreg];
	uint8_t s = types[t].size;
	int p = x86_phys_reg(f, vreg);

	if (!f->regs[vreg].uses) return;
	if (p >= 0) {
		store_phys(f, reg, (uint8_t)p, t);
		return;
	}
	cg_printf(f->cg, "\tmov%c %s, -%u(%s)\n",
			x86_mem_suf[s], reg_name(reg, s), f->regs[vreg].slot, f->base);
}

void x86_store_imm (X86Fn *f, uint32_t vreg, int64_t v)
{
	CodeGen *c = f->cg;
	uint8_t t = f->fn->reg_types[vreg];
	uint8_t s = types[t].size;
	int p = x86_phys_reg(f, vreg);

	if (p >= 0) {
		x86_load_imm(f, (uint8_t)p, norm_val((uint64_t)v, t));
		return;
	}
	if (s == 8 && v != (int32_t)v) {
		uint8_t w = x86_grab(f);
		cg_printf(c, "\tmovabsq $0x%llx, %s\n", (unsigned long long)v, reg_name(w, 8));
		x86_store_reg(f, w, vreg);
		return;
	}
	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;

	cg_printf(c, "\tmov%c $%lld, -%u(%s)\n",
			x86_mem_suf[s], (long long)v, f->regs[vreg].slot, f->base);
}

void x86_op_reg (X86Fn *f, const char *op, uint8_t w, uint32_t vreg)
{
	int is_const = f->regs[vreg].cstate == VR_CONST;
	int64_t v = is_const ? x86_const_ext(f, vreg) : 0;
	int p = x86_phys_reg(f, vreg);
	uint64_t keep = f->pool;

	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\t%sq $%lld, %s\n", op, (long long)v, reg_name(w, 8));
	} else if (p >= 0) {
		cg_printf(f->cg, "\t%sq %s, %s\n", op, reg_name(p, 8), reg_name(w, 8));
	} else if (!is_const && types[f->fn->reg_types[vreg]].size == 8) {
		cg_printf(f->cg, "\t%sq -%u(%s), %s\n", op,
				f->regs[vreg].slot, f->base, reg_name(w, 8));
	} else {
		uint8_t t = x86_grab(f);
		x86_load_reg(f, t, vreg);
		cg_printf(f->cg, "\t%sq %s, %s\n", op, reg_name(t, 8), reg_name(w, 8));
	}
	f->pool = keep;
}

int x86_op_reg_tmp (X86Fn *f, uint32_t vreg, int8_t home)
{
	if (f->regs[vreg].cstate == VR_CONST) {
		int64_t v = x86_const_ext(f, vreg);
		return v != (int32_t)v;
	}
	if (home >= 0) return 0;
	return types[f->fn->reg_types[vreg]].size != 8;
}
