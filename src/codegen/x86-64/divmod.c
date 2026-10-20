#include "codegen/x86-64/common.h"

static const char *sext_ops[9] = {
	[1] = "cbtw",
	[2] = "cwtd",
	[4] = "cltd",
	[8] = "cqto"
};

static uint64_t magic_unsigned (uint64_t d, unsigned *l)
{
	unsigned k = 64 - (unsigned)__builtin_clzll(d - 1);
	uint64_t low = k == 64 ? 0 - d : (1ull << k) - d;

	*l = k;
	return (uint64_t)(((unsigned __int128)low << 64) / d) + 1;
}

static uint64_t magic_signed (uint64_t d, unsigned *sh)
{
	uint64_t two63 = 1ull << 63;
	uint64_t anc = two63 - 1 - two63 % d;

	uint64_t q1 = two63 / anc;
	uint64_t r1 = two63 - q1 * anc;
	uint64_t q2 = two63 / d;
	uint64_t r2 = two63 - q2 * d;

	uint64_t delta;
	unsigned p = 63;

	do {
		p++;
		q1 <<= 1;
		r1 <<= 1;
		if (r1 >= anc) { q1++; r1 -= anc; }
		q2 <<= 1;
		r2 <<= 1;
		if (r2 >= d) { q2++; r2 -= d; }
		delta = d - r2;
	} while (q1 < delta || (q1 == delta && r1 == 0));

	*sh = p - 64;
	return q2 + 1;
}

static void make_mod_tail (X86Fn *f, uint8_t t, uint64_t d)
{
	CodeGen *c = f->cg;

	if (d <= INT32_MAX)
		cg_printf(c, "\timulq $%llu, %%rax, %%rax\n", (unsigned long long)d);
	else cg_printf(c, "\tmovabsq $0x%llx, %%rdx\n\timulq %%rdx, %%rax\n",
			(unsigned long long)d);
	cg_printf(c, "\tsubq %%rax, %s\n\tmovq %s, %%rax\n", reg_name(t, 8), reg_name(t, 8));
}

static void make_udiv_magic (X86Fn *f, uint8_t t, int is_div, uint64_t d)
{
	CodeGen *c = f->cg;
	const char *tn = reg_name(t, 8);
	unsigned l;
	uint64_t m = magic_unsigned(d, &l);

	cg_printf(c, "\tmovq %%rax, %s\n\tmovabsq $0x%llx, %%rdx\n\tmulq %%rdx\n",
			tn, (unsigned long long)m);
	cg_printf(c, "\tmovq %s, %%rax\n\tsubq %%rdx, %%rax\n\tshrq $1, %%rax\n", tn);
	cg_printf(c, "\taddq %%rdx, %%rax\n\tshrq $%u, %%rax\n", l - 1);
	if (!is_div) make_mod_tail(f, t, d);
}

static void make_sdiv_magic (X86Fn *f, uint8_t t, int is_div, uint64_t d)
{
	CodeGen *c = f->cg;
	const char *tn = reg_name(t, 8);
	unsigned s;
	uint64_t m = magic_signed(d, &s);

	cg_printf(c, "\tmovq %%rax, %s\n\tmovabsq $0x%llx, %%rdx\n\timulq %%rdx\n",
			tn, (unsigned long long)m);
	if ((int64_t)m < 0) cg_printf(c, "\taddq %s, %%rdx\n", tn);
	if (s) cg_printf(c, "\tsarq $%u, %%rdx\n", s);
	cg_printf(c, "\tmovq %%rdx, %%rax\n\tshrq $63, %%rax\n\taddq %%rdx, %%rax\n");
	if (!is_div) make_mod_tail(f, t, d);
}

static void make_div_pow2 (X86Fn *f, int sign, int is_div, int64_t v)
{
	CodeGen *c = f->cg;
	unsigned long long k = x86_shift_amt((uint64_t)v);

	if (!sign) {
		if (is_div) cg_printf(c, "\tshrq $%llu, %%rax\n", k);
		else cg_printf(c, "\tandq $%lld, %%rax\n", (long long)(v - 1));
		return;
	}
	cg_printf(c, "\tmovq %%rax, %%rdx\n\tsarq $63, %%rdx\n");
	cg_printf(c, "\tshrq $%llu, %%rdx\n\taddq %%rdx, %%rax\n", 64 - k);

	if (is_div) cg_printf(c, "\tsarq $%llu, %%rax\n", k);
	else cg_printf(c, "\tandq $%lld, %%rax\n\tsubq %%rdx, %%rax\n", (long long)(v - 1));
}

static int make_divmod_const (X86Fn *f, IRInstr *i)
{
	int sign = types[i->data_type].sign;
	int is_div = i->op == IR_DIV;

	if (f->regs[i->src2].cstate != VR_CONST) return 0;
	int64_t v = x86_const_ext(f, i->src2);
	if (v <= 0 || (sign && v == 1)) return 0;
	int pow2 = (v & (v - 1)) == 0;
	if (pow2 && !is_div && v - 1 != (int32_t)(v - 1)) return 0;

	x86_load_reg(f, RAX, i->src1);
	if (pow2) make_div_pow2(f, sign, is_div, v);
	else if (sign) make_sdiv_magic(f, x86_grab(f), is_div, (uint64_t)v);
	else make_udiv_magic(f, x86_grab(f), is_div, (uint64_t)v);

	x86_store_reg(f, RAX, i->dst);
	return 1;
}

void x86_make_divmod (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s = types[i->data_type].size;
	int sign = types[i->data_type].sign;

	x86_take_fixed(f, RAX, i->src1);
	x86_take_fixed(f, RDX, i->src2);
	if (make_divmod_const(f, i)) return;

	int p = x86_phys_reg(f, i->src2);
	if (p < 0 || p == RAX || p == RDX) {
		p = x86_grab(f);
		x86_load_reg(f, (uint8_t)p, i->src2);
	}
	x86_load_reg(f, RAX, i->src1);

	if (sign) cg_printf(c, "\t%s\n", sext_ops[s]);
	else if (s > 1) cg_printf(c, "\txorl %%edx, %%edx\n");
	cg_printf(c, "\t%s%c %s\n", sign ? "idiv" : "div", x86_mem_suf[s], reg_name(p, s));

	if (i->op == IR_DIV) x86_store_reg(f, RAX, i->dst);
	else if (s > 1) x86_store_reg(f, RDX, i->dst);
	else if (f->regs[i->dst].uses) {
		cg_printf(c, "\tmovzbl %%ah, %%eax\n");
		x86_store_reg(f, RAX, i->dst);
	}
}

