#include "codegen/x86_64.h"

#include <string.h>

#define SYM_PREFIX "__r2_"
#define INIT_SYM "__r2.init"
#define RED_ZONE 128

static uint32_t *count_uses (CodeGen *c, IRFn *fn)
{
	IR *ir = c->ir;
	size_t bytes = (size_t)fn->reg_count * sizeof(uint32_t);
	uint32_t *uses = arena_alloc(c->arena, bytes ? bytes : sizeof(uint32_t));

	memset(uses, 0, bytes);
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *instr = ir->instrs + fn->start + i;
		if (instr->src1 != NO_REG) uses[instr->src1]++;
		if (instr->src2 != NO_REG) uses[instr->src2]++;
	}
	return uses;
}

static void clasf_globals (CodeGen *c, uint8_t *kind, int64_t *vals)
{
	IR *ir = c->ir;
	IRFn *init = ir->fns + ir->init_fn;
	uint32_t *uses = count_uses(c, init);

	uint8_t *seen = arena_alloc(c->arena, ir->global_count ? ir->global_count : 1);
	memset(kind, 0, ir->global_count);
	memset(seen, 0, ir->global_count);

	for (uint32_t i = 0; i < init->count; i++) {

		IRInstr *instr = ir->instrs + init->start + i;
		if (instr->op == IR_CALL || instr->op == IR_JMP || instr->op == IR_JZ
				|| instr->op == IR_JNZ || instr->op == IR_LABEL) break;

		uint32_t g = NO_REG;
		int is_store = 0;

		if (instr->op == IR_STR_GLOBAL) {
			g = instr->target;
			is_store = 1;
		} else if (instr->op == IR_LD_GLOBAL) {
			g = instr->target;
		} else continue;

		if (seen[g]) continue;
		seen[g] = 1;

		if (!is_store || !i) continue;

		IRInstr *prev = instr - 1;
		if (prev->op != IR_CONST || prev->dst != instr->src1) continue;
		if (prev->data_type != ir->globals[g].type) continue;

		kind[g] = 1;

		vals[g] = prev->imm64;
		if (uses[prev->dst] == 1) prev->op = IR_NOP;
		instr->op = IR_NOP;
	}
}

static const char *asm_size_name[9] =
{
	[1] = ".byte",
	[2] = ".word",
	[4] = ".long",
	[8] = ".quad"
};

static void make_global_data (CodeGen *c, uint32_t i, int64_t val)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;

	cg_printf(c, "\t.balign %u\n" SYM_PREFIX "%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, "%s ", asm_size_name[size]);

	if (types[g->type].sign) cg_printf(c, "%lld\n", (long long)val);
	else cg_printf(c, "%llu\n", (unsigned long long)val);
}

static void make_global_bss (CodeGen *c, uint32_t i)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;
	cg_printf(c, "\t.balign %u\n" SYM_PREFIX "%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, ".zero %u\n", size);
}

static void make_globals (CodeGen *c, uint8_t *kind, int64_t *vals)
{
	IR *ir = c->ir;
	int printed = 0;
	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (!kind[i]) continue;
		if (!printed) {
			printed = 1;
			cg_printf(c, "\t.section .data\n");
		}
		make_global_data(c, i, vals[i]);
	}
	printed = 0;
	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (kind[i]) continue;
		if (!printed) {
			printed = 1;
			cg_printf(c, "\t.section .bss\n");
		}
		make_global_bss(c, i);
	}
}

static void scan_regs (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;

	memset(f->uses, 0, (size_t)fn->reg_count * sizeof(uint32_t));
	memset(f->cstate, 0, fn->reg_count);

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_NOP) continue;
		if (in->src1 != NO_REG) f->uses[in->src1]++;
		if (in->src2 != NO_REG) f->uses[in->src2]++;
		if (in->dst == NO_REG) continue;

		if (in->op == IR_CONST && f->cstate[in->dst] == 0) {
			f->cstate[in->dst] = 1;
			f->cval[in->dst] = in->imm64;
		} else f->cstate[in->dst] = 2;
	}
}

static uint32_t lay_size (X86Fn *f, uint32_t c, uint8_t s)
{
	IRFn *fn = f->fn;
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (types[fn->reg_types[r]].size != s || f->cstate[r] == 1) continue;
		c += s;
		f->slots[r] = c;
	}
	return c;
}

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
		f->leaf = 0;
		if (instr->argc <= 6) continue;
		n = (instr->argc - 6) << 3;
		if (n > out) out = n;
	}
	return out;
}

static void layout_fn (X86Fn *f)
{
	uint32_t c;
	scan_regs(f);
	c = lay_size(f, 0, 8);
	c = lay_size(f, c, 4);
	c = lay_size(f, c, 2);
	c = lay_size(f, c, 1);

	f->outgoing = scan_outg(f);
	c += f->outgoing;
	f->leaf = f->leaf && c <= RED_ZONE;
	f->frame = f->leaf ? 0 : (c + 15) & ~15u;
	f->base = f->leaf ? "%rsp" : "%rbp";
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
	uint8_t w;
	const char *op;

	if (f->cstate[vreg] == 1) {
		load_const(f, reg, const_ext(f, vreg));
		return;
	}

	op = load_op(f->fn->reg_types[vreg], &w);
	cg_printf(f->cg, "\t%s -%u(%s), %s\n",
			op, f->slots[vreg], f->base, reg_name(reg, w));
}

static void store_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	uint8_t s = types[f->fn->reg_types[vreg]].size;
	cg_printf(f->cg, "\tmov%c %s, -%u(%s)\n",
			mem_suf[s], reg_name(reg, s), f->slots[vreg], f->base);
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

static void make_arith (X86Fn *f, IRInstr *i)
{
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

static void make_divmod (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s = types[i->data_type].size;
	int sign = types[i->data_type].sign;

	load_reg(f, 0, i->src1);
	load_reg(f, 1, i->src2);

	if (sign) cg_printf(c, "\t%s\n", sext_ops[s]);
	else if (s > 1) cg_printf(c, "\txorl %%edx, %%edx\n");

	cg_printf(c, "\t%s%c %s\n", sign ? "idiv" : "div", mem_suf[s], reg_name(1, s));

	if (i->op == IR_DIV)
		store_reg(f, 0, i->dst);
	else if (s == 1)
		cg_printf(c, "\tmovb %%ah, -%u(%s)\n", f->slots[i->dst], f->base);
	else store_reg(f, 2, i->dst);
}

static void make_shift (X86Fn *f, IRInstr *i)
{
	const char *op = "shl";

	if (i->op == IR_RS)
		op = types[i->data_type].sign ? "sar" : "shr";

	load_reg(f, 0, i->src1);
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

static void make_cmp (X86Fn *f, IRInstr *i)
{
	int base = types[i->data_type].sign ? 0 : 6;
	const char *comp_name = comp_names[base + (i->op - IR_EQ)];

	load_reg(f, 0, i->src1);
	op_rax(f, "cmp", i->src2);
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
	load_reg(f, 0, cmp->src1);
	op_rax(f, "cmp", cmp->src2);
	cg_printf(f->cg, "\tj%s .L%u\n", comp_names[base + cc], jmp->target);
}

static void make_jump (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s;

	if (i->op == IR_LABEL) {
		cg_printf(c, ".L%u:\n", i->target);
		return;
	}
	if (i->op == IR_JMP) {
		cg_printf(c, "\tjmp .L%u\n", i->target);
		return;
	}
	if (f->cstate[i->src1] == 1) {
		int taken = (const_ext(f, i->src1) != 0) == (i->op == IR_JNZ);

		if (taken) cg_printf(c, "\tjmp .L%u\n", i->target);
		return;
	}
	s = types[f->fn->reg_types[i->src1]].size;
	cg_printf(c, "\tcmp%c $0, -%u(%s)\n", mem_suf[s], f->slots[i->src1], f->base);
	cg_printf(c, "\t%s .L%u\n", i->op == IR_JZ ? "je" : "jne", i->target);
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
}

static void make_ret (X86Fn *f, IRInstr *i)
{
	if (i->src1 != NO_REG)
		load_reg(f, 0, i->src1);
	cg_printf(f->cg, f->leaf ? "\tret\n" : "\tleave\n\tret\n");
}

static int make_instr (X86Fn *f, IRInstr *i)
{
	switch (i->op)
	{
	case IR_CONST:
		if (f->cstate[i->dst] != 1) store_imm(f, i->dst, i->imm64);
		return 1;
	case IR_PARAM: make_param(f, i); return 1;

	case IR_LD_GLOBAL: make_ld_global(f, i); return 1;
	case IR_STR_GLOBAL: make_str_global(f, i); return 1;
	case IR_MOVE: case IR_EXTEND: make_move(f, i); return 1;

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
	case IR_CALL: make_call(f, i); return 1;
	case IR_RET: make_ret(f, i); return 1;

	default: return 1;
	}
}

static void make_fn (X86Fn *f)
{
	CodeGen *c = f->cg;
	IR *ir = c->ir;
	IRFn *fn = f->fn;
	int dead = 0;

	cg_printf(c, "\t.p2align 4\n\t.globl ");
	print_fn_name(c, fn);
	cg_printf(c, "\n\t.type ");
	print_fn_name(c, fn);
	cg_printf(c, ", @function\n");
	print_fn_name(c, fn);

	cg_printf(c, ":\n");
	if (!f->leaf) cg_printf(c, "\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (f->frame) cg_printf(c, "\tsubq $%u, %%rsp\n", f->frame);

	uint32_t i = 0;
	while (i < fn->count) {
		IRInstr *instr = ir->instrs + fn->start + i;

		if (instr->op == IR_LABEL) dead = 0;
		if (dead) {
			i++;
			continue;
		}

		i += make_instr(f, instr);
		dead = instr->op == IR_RET || instr->op == IR_JMP;
	}

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
	IR *ir = c->ir;
	if (find_main(ir) == NO_REG) return;
	cg_printf(c, "\t.globl main\nmain:\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (run_init) cg_printf(c, "\tcall " INIT_SYM "\n");
	cg_printf(c, "\tcall " SYM_PREFIX "main\n\tpopq %%rbp\n\tret\n");
}

static int init_is_empty (CodeGen *c)
{
	IR *ir = c->ir;
	IRFn *init = ir->fns + ir->init_fn;

	for (uint32_t i = 0; i < init->count; i++) {
		uint8_t op = ir->instrs[init->start + i].op;
		if (op != IR_NOP && op != IR_RET) return 0;
	}
	return 1;
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;
	uint8_t *kind;
	int64_t *vals;
	int init_empty;

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
	kind = arena_alloc(c->arena, ir->global_count ? ir->global_count : 1);
	vals = arena_alloc(c->arena, (ir->global_count ? ir->global_count : 1) * sizeof(int64_t));

	clasf_globals(c, kind, vals);
	init_empty = init_is_empty(c);
	make_globals(c, kind, vals);
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
