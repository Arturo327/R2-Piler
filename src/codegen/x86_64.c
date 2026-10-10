#include "codegen/x86_64.h"

#include <string.h>

#define SYM_PREFIX "__r2_"
#define INIT_SYM "__r2.init"
#define RED_ZONE 128

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

static void make_globals (CodeGen *c)
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

static int is_silent (X86Fn *f, IRInstr *i)
{
	if (i->op == IR_NOP) return 1;
	if (i->dst == NO_REG) return 0;
	if (f->cstate[i->dst] == VR_CONST || f->cstate[i->dst] == VR_ALIAS)
		return 1;
	return f->uses[i->dst] == 0 && is_deletable[i->op];
}

static IRInstr *next_live (X86Fn *f, IRInstr *i, IRInstr *end)
{
	i++;
	while (i < end && is_silent(f, i)) i++;
	return i;
}

static const uint8_t leaves_in_rax[IR_COUNT] = {
	[IR_LD_GLOBAL] = 1, [IR_MOVE] = 1, [IR_EXTEND] = 1, [IR_CALL] = 1,
	[IR_ADD] = 1, [IR_SUB] = 1, [IR_MUL] = 1, [IR_DIV] = 1, [IR_MOD] = 1,
	[IR_AND_A] = 1, [IR_OR_A] = 1, [IR_XOR] = 1, [IR_RS] = 1, [IR_LS] = 1,
	[IR_NEG] = 1, [IR_NOT_A] = 1, [IR_NOT_L] = 1,
	[IR_EQ] = 1, [IR_NE] = 1, [IR_GT] = 1, [IR_GE] = 1, [IR_LT] = 1, [IR_LE] = 1
};

static const uint8_t reads_src1_first[IR_COUNT] = {
	[IR_MOVE] = 1, [IR_EXTEND] = 1, [IR_STR_GLOBAL] = 1,
	[IR_ADD] = 1, [IR_SUB] = 1, [IR_MUL] = 1, [IR_DIV] = 1, [IR_MOD] = 1,
	[IR_AND_A] = 1, [IR_OR_A] = 1, [IR_XOR] = 1, [IR_RS] = 1, [IR_LS] = 1,
	[IR_NEG] = 1, [IR_NOT_A] = 1, [IR_NOT_L] = 1,
	[IR_EQ] = 1, [IR_NE] = 1, [IR_GT] = 1, [IR_GE] = 1, [IR_LT] = 1, [IR_LE] = 1,
	[IR_JZ] = 1, [IR_JNZ] = 1, [IR_RET] = 1
};

static IRInstr *deferred_use (X86Fn *f, IRInstr *in, IRInstr *end)
{
	IRInstr *use = next_live(f, in, end);

	while (use != end && use->op == IR_ARG && use->src1 != in->dst)
		use = next_live(f, use, end);
	return use;
}

static int can_defer (X86Fn *f, IRInstr *in, IRInstr *end)
{
	uint32_t d = in->dst;

	if (d == NO_REG || !leaves_in_rax[in->op]) return 0;
	if (f->cstate[d] != VR_MEM || f->defs[d] != 1 || f->uses[d] != 1) return 0;
	if (in->op == IR_MOD && types[f->fn->reg_types[d]].size == 1) return 0;

	if ((in->op == IR_MOVE || in->op == IR_EXTEND)
			&& f->cstate[in->src1] == VR_CONST)
		return 0;

	IRInstr *use = deferred_use(f, in, end);
	if (use == end || use->src1 != d) return 0;
	return use->op == IR_ARG ? use->target < 6 : reads_src1_first[use->op];
}

static void mark_deferred (X86Fn *f)
{
	IRInstr *first = f->cg->ir->instrs + f->fn->start;
	IRInstr *end = first + f->fn->count;

	for (IRInstr *i = first; i < end; i++)
		if (can_defer(f, i, end)) f->cstate[i->dst] = VR_REG;
}

static int same_rep (X86Fn *f, uint32_t a, uint32_t b)
{
	const Type *ta = types + f->fn->reg_types[a];
	const Type *tb = types + f->fn->reg_types[b];

	return ta->size == tb->size && (ta->size == 8 || ta->sign == tb->sign);
}

static void count_defs (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;

	memset(f->uses, 0, (size_t)fn->reg_count * sizeof(uint32_t));
	memset(f->defs, 0, fn->reg_count);
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_NOP) continue;
		if (in->src1 != NO_REG) f->uses[in->src1]++;
		if (in->src2 != NO_REG) f->uses[in->src2]++;
		if (in->dst != NO_REG && f->defs[in->dst] < 2) f->defs[in->dst]++;
	}
}

static void clasf_def (X86Fn *f, IRInstr *in)
{
	uint32_t d = in->dst;
	uint32_t s = in->src1;

	f->cstate[d] = VR_MEM;
	if (f->defs[d] != 1) return;
	if (in->op == IR_CONST) {
		f->cstate[d] = VR_CONST;
		f->cval[d] = in->imm64;
		return;
	}
	if (in->op != IR_MOVE || !same_rep(f, d, s)) return;

	if (f->cstate[s] == VR_CONST) {
		f->cstate[d] = VR_CONST;
		f->cval[d] = f->cval[s];
	} else if (s < f->fn->param_count && f->cstate[s] == VR_MEM
			&& f->defs[s] == 1) {
		f->cstate[d] = VR_ALIAS;
		f->slots[d] = s;
	}
}

static void clasf_regs (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;

	memset(f->cstate, 0, fn->reg_count);
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op != IR_NOP && in->dst != NO_REG)
			clasf_def(f, in);
	}
}

static void scan_regs (X86Fn *f)
{
	count_defs(f);
	clasf_regs(f);
	mark_deferred(f);
}

static uint32_t lay_size (X86Fn *f, uint32_t c, uint8_t s)
{
	IRFn *fn = f->fn;

	for (uint32_t r = 0; r < fn->reg_count; r++) {
		uint8_t k = f->cstate[r];

		if (types[fn->reg_types[r]].size != s) continue;
		if (k == VR_CONST || k > VR_MEM || !f->uses[r]) continue;
		c += s;
		f->slots[r] = c;
	}
	return c;
}

static IRInstr *tail_ret (IRInstr *call);
static int tail_match (X86Fn *f, IRInstr *call, IRInstr *ret);

static uint32_t scan_outg (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	uint32_t out = 0;

	f->leaf = 1;
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *instr = ir->instrs + fn->start + i;
		uint32_t n;

		if (instr->op != IR_CALL) continue;
		if (tail_match(f, instr, tail_ret(instr))) continue;
		f->leaf = 0;

		if (instr->argc <= 6) continue;
		n = (instr->argc - 6) << 3;
		if (n > out) out = n;
	}
	return out;
}

static void resolve_alias (X86Fn *f)
{
	for (uint32_t r = 0; r < f->fn->reg_count; r++)
		if (f->cstate[r] == VR_ALIAS)
			f->slots[r] = f->slots[f->slots[r]];
}

static void layout_fn (X86Fn *f)
{
	uint32_t c;
	scan_regs(f);
	c = lay_size(f, 0, 8);
	c = lay_size(f, c, 4);
	c = lay_size(f, c, 2);
	c = lay_size(f, c, 1);
	resolve_alias(f);

	f->outgoing = scan_outg(f);
	c += f->outgoing;
	f->leaf = f->leaf && c <= RED_ZONE;
	f->frame = f->leaf ? 0 : (c + 15) & ~15u;
	f->base = f->leaf ? "%rsp" : "%rbp";

	f->rax_v = NO_REG;
}

static void print_fn_name (CodeGen *c, IRFn *fn)
{
	if (fn == c->ir->fns + c->ir->init_fn) cg_printf(c, INIT_SYM);
	else cg_printf(c, SYM_PREFIX "%.*s", (int)fn->len, fn->name);
}

static const char *regs_names[] =
{
	// 1 byte
	"%al", "%cl", "%dl", "%bl",
	"%spl", "%bpl", "%sil", "%dil",
	"%r8b", "%r9b", "%r10b", "%r11b",
	"%r12b", "%r13b", "%r14b", "%r15b",

	// 2 bytes
	"%ax", "%cx", "%dx", "%bx",
	"%sp", "%bp", "%si", "%di",
	"%r8w", "%r9w", "%r10w", "%r11w",
	"%r12w", "%r13w", "%r14w", "%r15w",

	// 4 bytes
	"%eax", "%ecx", "%edx", "%ebx",
	"%esp", "%ebp", "%esi", "%edi",
	"%r8d", "%r9d", "%r10d", "%r11d",
	"%r12d", "%r13d", "%r14d", "%r15d",

	// 8 bytes
	"%rax", "%rcx", "%rdx", "%rbx",
	"%rsp", "%rbp", "%rsi", "%rdi",
	"%r8", "%r9", "%r10", "%r11",
	"%r12", "%r13", "%r14", "%r15"
};

static const uint8_t size_idx[9] = { [1] = 0, [2] = 1, [4] = 2, [8] = 3 };
static const char mem_suf[9] = { [1] = 'b', [2] = 'w', [4] = 'l', [8] = 'q' };
static const uint8_t arg_regs[6] = { 7, 6, 2, 1, 8, 9 };

static inline const char *reg_name (uint8_t reg, uint8_t size)
{
	return regs_names[(size_idx[size] << 4) + reg];
}

static const char *const load_ops[8] = {
	"movzbl", "movsbq", "movzwl", "movswq",
	"movl", "movslq", "movq", "movq"
};

static const char *load_op (uint8_t t, uint8_t *size)
{
	uint8_t s = types[t].size;
	uint8_t sign = types[t].sign;

	*size = (s < 8 && !sign) ? 4 : 8;
	return load_ops[(size_idx[s] << 1) | sign];
}

static int64_t const_ext (X86Fn *f, uint32_t vreg)
{
	const Type *t = types + f->fn->reg_types[vreg];
	unsigned sh = 64u - t->size * 8u;
	uint64_t v = (uint64_t)f->cval[vreg] << sh;

	return t->sign ? (int64_t)v >> sh : (int64_t)(v >> sh);
}

static void load_const (X86Fn *f, uint8_t reg, int64_t v)
{
	CodeGen *c = f->cg;

	if (v >= 0 && v <= UINT32_MAX)
		cg_printf(c, "\tmovl $%lld, %s\n", (long long)v, reg_name(reg, 4));
	else if (v == (int32_t)v)
		cg_printf(c, "\tmovq $%lld, %s\n", (long long)v, reg_name(reg, 8));
	else cg_printf(c, "\tmovabsq $0x%llx, %s\n",
			(unsigned long long)v, reg_name(reg, 8));
}

static void load_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	if (reg == 0 && vreg == f->rax_v) return;

	if (reg && vreg == f->rax_v) {
		cg_printf(f->cg, "\tmovq %%rax, %s\n", reg_name(reg, 8));
		return;
	}

	if (f->cstate[vreg] == 1) {
		load_const(f, reg, const_ext(f, vreg));
		if (reg == 0) f->rax_v = vreg;
		return;
	}

	uint8_t w;
	const char *op = load_op(f->fn->reg_types[vreg], &w);
	cg_printf(f->cg, "\t%s -%u(%s), %s\n",
			op, f->slots[vreg], f->base, reg_name(reg, w));

	if (reg == 0) f->rax_v = vreg;
}

static const char *const norm_ops[6] = {
	"movzbl %al, %eax", "movsbq %al, %rax", "movzwl %ax, %eax",
	"movswq %ax, %rax", "movl %eax, %eax", "movslq %eax, %rax"
};

static void store_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	uint8_t s = types[f->fn->reg_types[vreg]].size;

	if (!f->uses[vreg]) {
		f->rax_v = NO_REG;
		return;
	}

	if (f->cstate[vreg] == VR_REG) {
		if (reg != 0)
			cg_printf(f->cg, "\tmovq %s, %%rax\n", reg_name(reg, 8));
		if (s < 8) cg_printf(f->cg, "\t%s\n", norm_ops[(size_idx[s] << 1)
				| types[f->fn->reg_types[vreg]].sign]);
		f->rax_v = vreg;
		return;
	}
	cg_printf(f->cg, "\tmov%c %s, -%u(%s)\n",
			mem_suf[s], reg_name(reg, s), f->slots[vreg], f->base);

	if (reg == 0 && s == 8) f->rax_v = vreg;
	else f->rax_v = NO_REG;
}

static void store_imm (X86Fn *f, uint32_t vreg, int64_t v)
{
	CodeGen *c = f->cg;
	uint8_t s = types[f->fn->reg_types[vreg]].size;

	if (s == 8 && v != (int32_t)v) {
		cg_printf(c, "\tmovabsq $0x%llx, %%rax\n", (unsigned long long)v);
		store_reg(f, 0, vreg);
		return;
	}

	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;

	cg_printf(c, "\tmov%c $%lld, -%u(%s)\n",
			mem_suf[s], (long long)v, f->slots[vreg], f->base);
	if (vreg == f->rax_v) f->rax_v = NO_REG;
}

static void make_param (X86Fn *f, IRInstr *i)
{
	if (i->target < 6) {
		store_reg(f, arg_regs[i->target], i->dst);
		return;
	}
	cg_printf(f->cg, "\tmovq %u(%s), %%rax\n",
			(f->leaf ? 8 : 16) + ((i->target - 6) << 3), f->base);
	store_reg(f, 0, i->dst);
}

static void make_move (X86Fn *f, IRInstr *i)
{
	if (i->dst == i->src1) return;
	if (f->cstate[i->src1] == 1) {
		store_imm(f, i->dst, const_ext(f, i->src1));
		return;
	}
	load_reg(f, 0, i->src1);
	store_reg(f, 0, i->dst);
}

static void make_ld_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;

	uint8_t size;
	const char *op = load_op(g->type, &size);

	cg_printf(c, "\t%s " SYM_PREFIX "%.*s(%%rip), %s\n", op,
			(int)g->len, g->name, reg_name(0, size));
	store_reg(f, 0, i->dst);
}

static void make_str_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;
	uint8_t s = types[g->type].size;

	if (f->cstate[i->src1] == 1) {
		int64_t v = const_ext(f, i->src1);

		if (s == 8 && v != (int32_t)v) {
			cg_printf(c, "\tmovabsq $0x%llx, %%rax\n", (unsigned long long)v);
			cg_printf(c, "\tmovq %%rax, " SYM_PREFIX "%.*s(%%rip)\n",
					(int)g->len, g->name);
			f->rax_v = NO_REG;
			return;
		}
		if (s == 4) v = (int32_t)v;
		else if (s == 2) v = (int16_t)v;
		else if (s == 1) v = (int8_t)v;
		cg_printf(c, "\tmov%c $%lld, " SYM_PREFIX "%.*s(%%rip)\n", mem_suf[s],
				(long long)v, (int)g->len, g->name);
		return;
	}

	load_reg(f, 0, i->src1);
	cg_printf(c, "\tmov%c %s, " SYM_PREFIX "%.*s(%%rip)\n", mem_suf[s],
			reg_name(0, s), (int)g->len, g->name);
}

static void op_rax (X86Fn *f, const char *op, uint32_t vreg)
{
	int is_const = f->cstate[vreg] == 1;
	int64_t v = is_const ? const_ext(f, vreg) : 0;

	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\t%sq $%lld, %%rax\n", op, (long long)v);
		return;
	}
	if (!is_const && types[f->fn->reg_types[vreg]].size == 8) {
		cg_printf(f->cg, "\t%sq -%u(%s), %%rax\n", op, f->slots[vreg], f->base);
		return;
	}
	load_reg(f, 1, vreg);
	cg_printf(f->cg, "\t%sq %%rcx, %%rax\n", op);
}

static const char *arith_ops[IR_COUNT] = {
	[IR_ADD] = "add",
	[IR_SUB] = "sub",
	[IR_MUL] = "imul",
	[IR_AND_A] = "and",
	[IR_OR_A] = "or",
	[IR_XOR] = "xor"
};

static uint64_t shift_amt (uint64_t v)
{
	uint64_t k = 0;

	while ((v & 1u) == 0) {
		v >>= 1;
		k++;
	}
	return k;
}

static void make_mul_pm1 (X86Fn *f, uint64_t k, int plus)
{
	CodeGen *c = f->cg;

	cg_printf(c, "\tmovq %%rax, %%rdx\n\tshlq $%llu, %%rdx\n", (unsigned long long)k);
	if (plus) cg_printf(c, "\taddq %%rdx, %%rax\n");
	else cg_printf(c, "\tsubq %%rax, %%rdx\n\tmovq %%rdx, %%rax\n");
}

static int make_mul_const (X86Fn *f, IRInstr *i)
{
	if (f->cstate[i->src2] != 1) return 0;
	int64_t v = const_ext(f, i->src2);
	load_reg(f, 0, i->src1);

	if (v > 0 && (v & (v - 1)) == 0) {
		uint64_t k = shift_amt((uint64_t)v);
		cg_printf(f->cg, "\tshlq $%llu, %%rax\n", (unsigned long long)k);
	} else if (v == 3 || v == 5 || v == 9)
		cg_printf(f->cg, "\tleaq (%%rax,%%rax,%lld), %%rax\n", (long long)(v - 1));
	else if (v > 9 && !((uint64_t)(v - 1) & (uint64_t)(v - 2)))
		make_mul_pm1(f, shift_amt((uint64_t)(v - 1)), 1);
	else if (v > 3 && !((uint64_t)v & ((uint64_t)v + 1)))
		make_mul_pm1(f, shift_amt((uint64_t)v + 1), 0);
	else op_rax(f, "imul", i->src2);

	store_reg(f, 0, i->dst);
	return 1;
}

static void make_arith (X86Fn *f, IRInstr *i)
{
	if (i->op == IR_MUL && make_mul_const(f, i)) return;
	load_reg(f, 0, i->src1);
	op_rax(f, arith_ops[i->op], i->src2);
	store_reg(f, 0, i->dst);
}

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

static void make_mod_tail (X86Fn *f, uint64_t d)
{
	CodeGen *c = f->cg;

	if (d <= INT32_MAX)
		cg_printf(c, "\timulq $%llu, %%rax, %%rax\n", (unsigned long long)d);
	else cg_printf(c, "\tmovabsq $0x%llx, %%rdx\n\timulq %%rdx, %%rax\n",
			(unsigned long long)d);
	cg_printf(c, "\tsubq %%rax, %%rcx\n\tmovq %%rcx, %%rax\n");
}

static void make_udiv_magic (X86Fn *f, int is_div, uint64_t d)
{
	CodeGen *c = f->cg;
	unsigned l;
	uint64_t m = magic_unsigned(d, &l);

	cg_printf(c, "\tmovq %%rax, %%rcx\n\tmovabsq $0x%llx, %%rdx\n\tmulq %%rdx\n",
			(unsigned long long)m);
	cg_printf(c, "\tmovq %%rcx, %%rax\n\tsubq %%rdx, %%rax\n\tshrq $1, %%rax\n");
	cg_printf(c, "\taddq %%rdx, %%rax\n\tshrq $%u, %%rax\n", l - 1);
	if (!is_div) make_mod_tail(f, d);
}

static void make_sdiv_magic (X86Fn *f, int is_div, uint64_t d)
{
	CodeGen *c = f->cg;
	unsigned s;
	uint64_t m = magic_signed(d, &s);

	cg_printf(c, "\tmovq %%rax, %%rcx\n\tmovabsq $0x%llx, %%rdx\n\timulq %%rdx\n",
			(unsigned long long)m);
	if ((int64_t)m < 0) cg_printf(c, "\taddq %%rcx, %%rdx\n");
	if (s) cg_printf(c, "\tsarq $%u, %%rdx\n", s);
	cg_printf(c, "\tmovq %%rdx, %%rax\n\tshrq $63, %%rax\n\taddq %%rdx, %%rax\n");
	if (!is_div) make_mod_tail(f, d);
}

static void make_div_pow2 (X86Fn *f, int sign, int is_div, int64_t v)
{
	CodeGen *c = f->cg;
	unsigned long long k = shift_amt((uint64_t)v);

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

	if (f->cstate[i->src2] != VR_CONST) return 0;
	int64_t v = const_ext(f, i->src2);
	if (v <= 0 || (sign && v == 1)) return 0;
	int pow2 = (v & (v - 1)) == 0;
	if (pow2 && !is_div && v - 1 != (int32_t)(v - 1)) return 0;

	load_reg(f, 0, i->src1);
	if (pow2) make_div_pow2(f, sign, is_div, v);
	else if (sign) make_sdiv_magic(f, is_div, (uint64_t)v);
	else make_udiv_magic(f, is_div, (uint64_t)v);

	store_reg(f, 0, i->dst);
	return 1;
}

static void make_divmod (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s = types[i->data_type].size;
	int sign = types[i->data_type].sign;

	if (make_divmod_const(f, i)) return;
	load_reg(f, 0, i->src1);
	load_reg(f, 1, i->src2);

	if (sign) cg_printf(c, "\t%s\n", sext_ops[s]);
	else if (s > 1) cg_printf(c, "\txorl %%edx, %%edx\n");

	cg_printf(c, "\t%s%c %s\n", sign ? "idiv" : "div", mem_suf[s], reg_name(1, s));
	f->rax_v = NO_REG;

	if (i->op == IR_DIV)
		store_reg(f, 0, i->dst);
	else if (s == 1) {
		if (f->uses[i->dst])
			cg_printf(c, "\tmovb %%ah, -%u(%s)\n", f->slots[i->dst], f->base);
		f->rax_v = NO_REG;
	} else store_reg(f, 2, i->dst);
}

static void make_shift (X86Fn *f, IRInstr *i)
{
	const char *op = "shl";

	if (i->op == IR_RS)
		op = types[i->data_type].sign ? "sar" : "shr";

	load_reg(f, 0, i->src1);
	if (f->cstate[i->src2] == 1) {
		int64_t v = const_ext(f, i->src2);
		if (v >= 0 && v < 64) {
			cg_printf(f->cg, "\t%sq $%lld, %%rax\n", op, (long long)v);
			store_reg(f, 0, i->dst);
			return;
		}
	}

	load_reg(f, 1, i->src2);
	cg_printf(f->cg, "\t%sq %%cl, %%rax\n", op);
	store_reg(f, 0, i->dst);
}

static void make_unary (X86Fn *f, IRInstr *i, const char *op)
{
	load_reg(f, 0, i->src1);
	cg_printf(f->cg, "\t%sq %%rax\n", op);
	store_reg(f, 0, i->dst);
}

static void make_not_l (X86Fn *f, IRInstr *i)
{
	load_reg(f, 0, i->src1);
	cg_printf(f->cg, "\ttestq %%rax, %%rax\n\tsete %%al\n\tmovzbl %%al, %%eax\n");
	store_reg(f, 0, i->dst);
}

static const char *comp_names[12] = {
	"e", "ne", "g", "ge", "l", "le",
	"e", "ne", "a", "ae", "b", "be"
};

static int cmp_mem_imm (X86Fn *f, IRInstr *i)
{
	uint32_t a = i->src1;
	uint8_t s = types[f->fn->reg_types[a]].size;
	int64_t v;

	if (f->cstate[i->src2] != VR_CONST || a == f->rax_v) return 0;
	if (f->cstate[a] != VR_MEM && f->cstate[a] != VR_ALIAS) return 0;
	v = const_ext(f, i->src2);
	if (s == 8 && v != (int32_t)v) return 0;
	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;
	cg_printf(f->cg, "\tcmp%c $%lld, -%u(%s)\n", mem_suf[s],
			(long long)v, f->slots[a], f->base);
	return 1;
}

static void write_cmp (X86Fn *f, IRInstr *i)
{
	if (cmp_mem_imm(f, i)) return;
	load_reg(f, 0, i->src1);
	op_rax(f, "cmp", i->src2);
}

static void make_cmp (X86Fn *f, IRInstr *i)
{
	int base = types[i->data_type].sign ? 0 : 6;
	const char *comp_name = comp_names[base + (i->op - IR_EQ)];

	write_cmp(f, i);
	cg_printf(f->cg, "\tset%s %%al\n\tmovzbl %%al, %%eax\n", comp_name);
	store_reg(f, 0, i->dst);
}

static const uint8_t inv_cmp[6] = { 1, 0, 5, 4, 3, 2 };

static int fuses_with_next (X86Fn *f, IRInstr *i)
{
	IRInstr *n = i + 1;

	if (n->op != IR_JZ && n->op != IR_JNZ) return 0;
	return n->src1 == i->dst && f->uses[i->dst] == 1;
}

static void make_cmp_jump (X86Fn *f, IRInstr *cmp, IRInstr *jmp)
{
	int base = types[cmp->data_type].sign ? 0 : 6;
	int cc = cmp->op - IR_EQ;

	if (jmp->op == IR_JZ) cc = inv_cmp[cc];
	write_cmp(f, cmp);
	cg_printf(f->cg, "\tj%s .L%u\n", comp_names[base + cc], jmp->target);
}

static void make_jump (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s;

	if (i->op == IR_LABEL) {
		cg_printf(c, ".L%u:\n", i->target);
		f->rax_v = NO_REG;
		return;
	}
	if (i->op == IR_JMP) {
		cg_printf(c, "\tjmp .L%u\n", i->target);
		f->rax_v = NO_REG;
		return;
	}
	if (f->cstate[i->src1] == 1) {
		int taken = (const_ext(f, i->src1) != 0) == (i->op == IR_JNZ);
		if (taken) cg_printf(c, "\tjmp .L%u\n", i->target);
		f->rax_v = NO_REG;
		return;
	}

	if (i->src1 == f->rax_v) {
		cg_printf(c, "\ttestq %%rax, %%rax\n");
	} else {
		s = types[f->fn->reg_types[i->src1]].size;
		cg_printf(c, "\tcmp%c $0, -%u(%s)\n", mem_suf[s], f->slots[i->src1], f->base);
	}

	cg_printf(c, "\t%s .L%u\n", i->op == IR_JZ ? "je" : "jne", i->target);
	f->rax_v = NO_REG;
}

static void make_arg (X86Fn *f, IRInstr *i)
{
	int is_const = f->cstate[i->src1] == 1;
	int64_t v = is_const ? const_ext(f, i->src1) : 0;

	if (i->target < 6) {
		load_reg(f, arg_regs[i->target], i->src1);
		return;
	}
	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\tmovq $%lld, %u(%%rsp)\n",
				(long long)v, (i->target - 6) << 3);
		return;
	}
	load_reg(f, 0, i->src1);
	cg_printf(f->cg, "\tmovq %%rax, %u(%%rsp)\n", (i->target - 6) << 3);
}

static void make_call (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;

	cg_printf(c, "\tcall ");
	print_fn_name(c, c->ir->fns + i->target);
	cg_printf(c, "\n");
	if (i->dst != NO_REG)
		store_reg(f, 0, i->dst);
	else f->rax_v = NO_REG;
}

static void make_ret (X86Fn *f, IRInstr *i)
{
	if (i->src1 != NO_REG)
		load_reg(f, 0, i->src1);
	cg_printf(f->cg, f->leaf ? "\tret\n" : "\tleave\n\tret\n");
}

enum { RMW_NONE = 0, RMW_UNARY, RMW_ARITH, RMW_SHIFT };

static const uint8_t rmw_kind[IR_COUNT] = {
	[IR_ADD] = RMW_ARITH, [IR_SUB] = RMW_ARITH, [IR_AND_A] = RMW_ARITH,
	[IR_OR_A] = RMW_ARITH, [IR_XOR] = RMW_ARITH,
	[IR_NEG] = RMW_UNARY, [IR_NOT_A] = RMW_UNARY,
	[IR_MUL] = RMW_SHIFT, [IR_LS] = RMW_SHIFT, [IR_RS] = RMW_SHIFT
};

static const char *const rmw_names[IR_COUNT] = {
	[IR_ADD] = "add", [IR_SUB] = "sub", [IR_AND_A] = "and",
	[IR_OR_A] = "or", [IR_XOR] = "xor", [IR_NEG] = "neg", [IR_NOT_A] = "not"
};

static int rmw_shift (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	int64_t k;

	if (f->cstate[b->src2] != VR_CONST) return 0;
	k = const_ext(f, b->src2);
	if (b->op == IR_MUL) {
		if (k <= 1 || (k & (k - 1))) return 0;
		k = (int64_t)shift_amt((uint64_t)k);
	}
	if (k < 1 || k >= 8 * s) return 0;
	if (b->op == IR_RS) *op = types[b->data_type].sign ? "sar" : "shr";
	else *op = "shl";
	*v = k;
	return 1;
}

static int rmw_arith (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	int64_t k;

	if (f->cstate[b->src2] != VR_CONST) return 0;
	k = const_ext(f, b->src2);
	if (s == 8 && k != (int32_t)k) return 0;
	if (s == 4) k = (int32_t)k;
	else if (s == 2) k = (int16_t)k;
	else if (s == 1) k = (int8_t)k;
	*op = rmw_names[b->op];
	*v = k;
	return 1;
}

static int rmw_operand (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	switch (rmw_kind[b->op])
	{
	case RMW_UNARY: *op = rmw_names[b->op]; return 1;
	case RMW_ARITH: return rmw_arith(f, b, s, op, v);
	case RMW_SHIFT: return rmw_shift(f, b, s, op, v);
	default: return 0;
	}
}

static int rmw_closes (IRInstr *a, IRInstr *c)
{
	if (a->op == IR_LD_GLOBAL)
		return c->op == IR_STR_GLOBAL && c->target == a->target;
	return c->op == IR_MOVE && c->dst == a->src1;
}

static IRInstr *rmw_find (X86Fn *f, IRInstr *a, IRInstr **mid)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *b = next_live(f, a, end);

	if (b == end || !rmw_kind[b->op] || b->src1 != a->dst) return NULL;
	IRInstr *c = next_live(f, b, end);
	if (c == end || c->src1 != b->dst || !rmw_closes(a, c)) return NULL;

	*mid = b;
	return c;
}

static uint8_t rmw_width (X86Fn *f, IRInstr *a)
{
	if (a->op == IR_LD_GLOBAL)
		return types[f->cg->ir->globals[a->target].type].size;
	return types[f->fn->reg_types[a->src1]].size;
}

static int rmw_shape_ok (X86Fn *f, IRInstr *a, IRInstr *b)
{
	uint8_t *rt = f->fn->reg_types;
	uint8_t s = rmw_width(f, a);

	if (f->uses[a->dst] != 1 || f->uses[b->dst] != 1) return 0;
	if (a->op != IR_LD_GLOBAL && f->cstate[a->src1] != VR_MEM) return 0;
	return types[rt[a->dst]].size == s && types[rt[b->dst]].size == s;
}

static void print_rmw_loc (X86Fn *f, IRInstr *a)
{
	IRGlobal *g;

	if (a->op != IR_LD_GLOBAL) {
		cg_printf(f->cg, "-%u(%s)\n", f->slots[a->src1], f->base);
		return;
	}
	g = f->cg->ir->globals + a->target;
	cg_printf(f->cg, SYM_PREFIX "%.*s(%%rip)\n", (int)g->len, g->name);
}

static int make_rmw (X86Fn *f, IRInstr *a)
{
	IRInstr *b = NULL;
	IRInstr *c = rmw_find(f, a, &b);
	const char *op = NULL;
	int64_t v = 0;

	if (!c || !rmw_shape_ok(f, a, b)) return 0;
	uint8_t s = rmw_width(f, a);
	if (!rmw_operand(f, b, s, &op, &v)) return 0;

	cg_printf(f->cg, "\t%s%c ", op, mem_suf[s]);
	if (rmw_kind[b->op] != RMW_UNARY)
		cg_printf(f->cg, "$%lld, ", (long long)v);
	print_rmw_loc(f, a);

	if (f->rax_v == a->src1) f->rax_v = NO_REG;
	return (int)(c - a) + 1;
}

static int try_rmw_inplace (X86Fn *f, IRInstr *i)
{
	const char *op = NULL;
	int64_t v = 0;
	uint32_t x = i->dst;
	uint8_t s;

	if (!rmw_kind[i->op] || i->dst != i->src1 || f->cstate[x] != VR_MEM)
		return 0;
	s = types[f->fn->reg_types[x]].size;
	if (!rmw_operand(f, i, s, &op, &v)) return 0;

	cg_printf(f->cg, "\t%s%c ", op, mem_suf[s]);
	if (rmw_kind[i->op] != RMW_UNARY)
		cg_printf(f->cg, "$%lld, ", (long long)v);
	cg_printf(f->cg, "-%u(%s)\n", f->slots[x], f->base);

	if (f->rax_v == x) f->rax_v = NO_REG;
	return 1;
}

static IRInstr *tail_ret (IRInstr *call)
{
	IRInstr *n = call + 1;

	while (n->op == IR_LABEL) n++;
	return n;
}

static int tail_match (X86Fn *f, IRInstr *call, IRInstr *ret)
{
	uint8_t from = f->cg->ir->fns[call->target].ret_type;
	uint8_t to = f->fn->ret_type;

	if (ret->op != IR_RET || call->argc > 6) return 0;
	if (ret->src1 != call->dst) return 0;
	if (call->dst != NO_REG && f->uses[call->dst] != 1) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

static int make_tail_call (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRInstr *ret = tail_ret(i);
	if (!tail_match(f, i, ret)) return 0;

	if (c->ir->fns + i->target == f->fn) {
		cg_printf(c, "\tjmp .LS%u\n", i->target);
	} else {
		if (!f->leaf) cg_printf(c, "\tleave\n");
		cg_printf(c, "\tjmp ");
		print_fn_name(c, c->ir->fns + i->target);
		cg_printf(c, "\n");
	}

	f->rax_v = NO_REG;
	return ret == i + 1 ? 2 : 1;
}

static int make_instr (X86Fn *f, IRInstr *i)
{
	if (is_silent(f, i)) return 1;
	int n;
	if (try_rmw_inplace(f, i)) return 1;

	switch (i->op)
	{
	case IR_CONST:
		if (f->cstate[i->dst] != 1) store_imm(f, i->dst, i->imm64);
		return 1;
	case IR_PARAM: make_param(f, i); return 1;

	case IR_LD_GLOBAL:
		n = make_rmw(f, i);
		if (n) return n;
		make_ld_global(f, i);
		return 1;
	case IR_STR_GLOBAL: make_str_global(f, i); return 1;

	case IR_MOVE:
		n = make_rmw(f, i);
		if (n) return n;
		make_move(f, i);
		return 1;
	case IR_EXTEND: make_move(f, i); return 1;

	case IR_ADD: case IR_SUB: case IR_AND_A: case IR_OR_A: case IR_MUL: case IR_XOR:
		make_arith(f, i); return 1;
	case IR_DIV: case IR_MOD: make_divmod(f, i); return 1;
	case IR_RS: case IR_LS: make_shift(f, i); return 1;

	case IR_NEG: make_unary(f, i, "neg"); return 1;
	case IR_NOT_A: make_unary(f, i, "not"); return 1;
	case IR_NOT_L: make_not_l(f, i); return 1;

	case IR_EQ: case IR_NE: case IR_GT: case IR_GE: case IR_LT: case IR_LE:
		if (!fuses_with_next(f, i)) {
			make_cmp(f, i);
			return 1;
		}
		make_cmp_jump(f, i, i + 1);
		return 2;

	case IR_LABEL: case IR_JMP: case IR_JZ: case IR_JNZ: make_jump(f, i); return 1;

	case IR_ARG: make_arg(f, i); return 1;
	case IR_CALL:
		n = make_tail_call(f, i);
		if (n) return n;
		make_call(f, i);
		return 1;
	case IR_RET: make_ret(f, i); return 1;

	default: return 1;
	}
}

static void make_body (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	IRInstr *instr, *last;
	uint32_t i = 0, n;
	int dead = 0;

	while (i < fn->count) {
		instr = ir->instrs + fn->start + i;
		if (instr->op == IR_LABEL) dead = 0;
		if (dead) {
			i++;
			continue;
		}
		n = make_instr(f, instr);
		last = instr + n - 1;
		dead = last->op == IR_RET || last->op == IR_JMP;
		i += n;
	}
}

static void make_fn (X86Fn *f)
{
	CodeGen *c = f->cg;
	IRFn *fn = f->fn;

	cg_printf(c, "\t.p2align 4\n\t.globl ");
	print_fn_name(c, fn);
	cg_printf(c, "\n\t.type ");
	print_fn_name(c, fn);
	cg_printf(c, ", @function\n");
	print_fn_name(c, fn);

	cg_printf(c, ":\n");
	if (!f->leaf) cg_printf(c, "\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (f->frame) cg_printf(c, "\tsubq $%u, %%rsp\n", f->frame);
	cg_printf(c, ".LS%u:\n", (uint32_t)(fn - c->ir->fns));

	make_body(f);

	cg_printf(c, "\t.size ");
	print_fn_name(c, fn);
	cg_printf(c, ", .-");
	print_fn_name(c, fn);
	cg_printf(c, "\n");
}

static uint32_t find_main (IR *ir)
{
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (ir->fns[i].len == 4 && !strncmp(ir->fns[i].name, "main", 4)) return i;
	return NO_REG;
}

static void make_entry (CodeGen *c, int run_init)
{
	if (find_main(c->ir) == NO_REG) return;
	if (run_init) {
		cg_printf(c, "\t.globl main\nmain:\n\tsubq $8, %%rsp\n");
		cg_printf(c, "\tcall " INIT_SYM "\n\taddq $8, %%rsp\n");
		cg_printf(c, "\tjmp " SYM_PREFIX "main\n");
		return;
	}
	cg_printf(c, "\t.globl main\nmain:\n\tjmp " SYM_PREFIX "main\n");
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;

	uint32_t max_regs = 1;
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (ir->fns[i].reg_count > max_regs)
			max_regs = ir->fns[i].reg_count;

	X86Fn f = {0};
	f.slots = arena_alloc(c->arena, (size_t)max_regs * sizeof(uint32_t));
	f.cg = c;

	f.uses = arena_alloc(c->arena, (size_t)max_regs * sizeof(uint32_t));
	f.cval = arena_alloc(c->arena, (size_t)max_regs * sizeof(int64_t));
	f.cstate = arena_alloc(c->arena, max_regs);
	f.defs = arena_alloc(c->arena, max_regs);

	int init_empty = ir->fns[ir->init_fn].count <= 1;
	make_globals(c);
	cg_printf(c, "\t.text\n");

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (init_empty && i == ir->init_fn) continue;
		f.fn = ir->fns + i;
		layout_fn(&f);
		make_fn(&f);
	}
	make_entry(c, !init_empty);

	cg_printf(c, "\t.section .note.GNU-stack,\"\",@progbits\n");

	return 0;
}
