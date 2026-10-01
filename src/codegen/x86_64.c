#include "codegen/x86_64.h"

#include <string.h>

#define SYM_PREFIX "__r2_"
#define INIT_SYM "__r2.init"

static void clasf_globals (CodeGen *c, uint8_t *kind, int64_t *vals)
{
	IR *ir = c->ir;
	IRFn *init = ir->fns + ir->init_fn;

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

static uint32_t lay_size (X86Fn *f, uint32_t c, uint8_t s)
{
	IRFn *fn = f->fn;
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (types[fn->reg_types[r]].size != s) continue;
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

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *instr = ir->instrs + fn->start + i;
		uint32_t n;
		if (instr->op != IR_CALL || instr->argc <= 6) continue;
		n = (instr->argc - 6) << 3;
		if (n > out) out = n;
	}
	return out;
}

static void layout_fn (X86Fn *f)
{
	uint32_t c = lay_size(f, 0, 8);
	c = lay_size(f, c, 4);
	c = lay_size(f, c, 2);
	c = lay_size(f, c, 1);
	f->outgoing = scan_outg(f);
	c += f->outgoing;
	f->frame = (c + 15) & ~15u;
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

static void load_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	uint8_t w;
	const char *op = load_op(f->fn->reg_types[vreg], &w);
	cg_printf(f->cg, "\t%s -%u(%%rbp), %s\n",
			op, f->slots[vreg], reg_name(reg, w));
}

static void store_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	uint8_t s = types[f->fn->reg_types[vreg]].size;
	cg_printf(f->cg, "\tmov%c %s, -%u(%%rbp)\n",
			mem_suf[s], reg_name(reg, s), f->slots[vreg]);
}

static void make_const (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s = types[f->fn->reg_types[i->dst]].size;
	int64_t v = i->imm64;

	if (s == 8 && (v < INT32_MIN || v > INT32_MAX)) {
		cg_printf(c, "\tmovabsq $0x%llx, %%rax\n", (unsigned long long)v);
		store_reg(f, 0, i->dst);
		return;
	}
	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;

	cg_printf(c, "\tmov%c $%lld, -%u(%%rbp)\n",
			mem_suf[s], (long long)v, f->slots[i->dst]);
}

static void make_param (X86Fn *f, IRInstr *i)
{
	if (i->target < 6) {
		store_reg(f, arg_regs[i->target], i->dst);
		return;
	}
	cg_printf(f->cg, "\tmovq %u(%%rbp), %%rax\n", 16 + ((i->target - 6) << 3));
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

static void make_move (X86Fn *f, IRInstr *i)
{
	load_reg(f, 0, i->src1);
	store_reg(f, 0, i->dst);
}

static void op_rax (X86Fn *f, const char *op, uint32_t vreg)
{
	if (types[f->fn->reg_types[vreg]].size == 8) {
		cg_printf(f->cg, "\t%sq -%u(%%rbp), %%rax\n", op, f->slots[vreg]);
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
		cg_printf(c, "\tmovb %%ah, -%u(%%rbp)\n", f->slots[i->dst]);
	else store_reg(f, 2, i->dst);
}

static void make_instr (X86Fn *f, IRInstr *i)
{
	switch (i->op)
	{
	case IR_CONST: make_const(f, i); break;
	case IR_PARAM: make_param(f, i); break;
	case IR_LD_GLOBAL: make_ld_global(f, i); break;
	case IR_STR_GLOBAL: make_str_global(f, i); break;
	case IR_MOVE: case IR_EXTEND: make_move(f, i); break;

	case IR_ADD: case IR_SUB: case IR_AND_A: case IR_OR_A: case IR_MUL: case IR_XOR:
		make_arith(f, i); break;
	case IR_DIV: case IR_MOD: make_divmod(f, i); break;
/*
	case IR_RS: case IR_LS: make_shift(f, i); break;
	case IR_NEG: make_unary(f, i, "neg"); break;
	case IR_NOT_A: make_unary(f, i, "not"); break;
	case IR_NOT_L: make_not_l(f, i); break;
	case IR_EQ: case IR_NE: case IR_GT: case IR_GE: case IR_LT: case IR_LE:
		make_cmp(f, i); break;
	case IR_LABEL: case IR_JMP: case IR_JZ: case IR_JNZ: make_jump(f, i); break;
	case IR_ARG: make_arg(f, i); break;
	case IR_CALL: make_call(f, i); break;
	case IR_RET: make_ret(f, i); break;
*/
	default: break;
	}
}

static void make_fn (X86Fn *f)
{
	CodeGen *c = f->cg;
	IR *ir = c->ir;
	IRFn *fn = f->fn;

	cg_printf(c, "\t.p2align 4\n\t.globl ");
	print_fn_name(c, fn);
	cg_printf(c, "\n");
	print_fn_name(c, fn);

	cg_printf(c, ":\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (f->frame) cg_printf(c, "\tsubq $%u, %%rsp\n", f->frame);

	for (uint32_t i = 0; i < fn->count; i++)
		make_instr(f, ir->instrs + fn->start + i);
}

static uint32_t find_main (IR *ir)
{
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (ir->fns[i].len == 4 && !strncmp(ir->fns[i].name, "main", 4)) return i;
	return NO_REG;
}

static void make_entry (CodeGen *c)
{
	IR *ir = c->ir;
	if (find_main(ir) == NO_REG) return;
	cg_printf(c, "\t.globl main\nmain:\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (ir->global_count) cg_printf(c, "\tcall " INIT_SYM "\n");
	cg_printf(c, "\tcall " SYM_PREFIX "main\n\tpopq %%rbp\n\tret\n");
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;
	uint8_t *kind;
	int64_t *vals;

	uint32_t max_regs = 1;
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (ir->fns[i].reg_count > max_regs)
			max_regs = ir->fns[i].reg_count;

	X86Fn f = {0};
	f.slots = arena_alloc(c->arena, (size_t)max_regs * sizeof(uint32_t));
	f.cg = c;

	kind = arena_alloc(c->arena, ir->global_count ? ir->global_count : 1);
	vals = arena_alloc(c->arena, (ir->global_count ? ir->global_count : 1)
			* sizeof(int64_t));

	clasf_globals(c, kind, vals);
	make_globals(c, kind, vals);
	cg_printf(c, "\t.text\n");

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		f.fn = ir->fns + i;
		layout_fn(&f);
		make_fn(&f);
	}
	make_entry(c);
	cg_printf(c, "\t.section .note.GNU-stack,\"\",@progbits\n");

	return 0;
}
