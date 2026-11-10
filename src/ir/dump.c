#include "ir/ir.h"

#include <stdio.h>

static const char *const op_names[IR_COUNT] = {
	[IR_NOP] = "nop", [IR_CONST] = "const", [IR_PARAM] = "param",
	[IR_LD_GLOBAL] = "ld_global", [IR_STR_GLOBAL] = "str_global",
	[IR_ADDR_LOCAL] = "addr_local", [IR_ADDR_GLOBAL] = "addr_global",
	[IR_LD_PTR] = "ld_ptr", [IR_STR_PTR] = "str_ptr", [IR_STR_LIT] = "str_lit",
	[IR_LD_LOCAL] = "ld_local", [IR_STR_LOCAL] = "str_local",
	[IR_MOVE] = "move", [IR_EXTEND] = "extend", [IR_SELECT] = "select",
	[IR_ADD] = "add", [IR_SUB] = "sub", [IR_MUL] = "mul",
	[IR_DIV] = "div", [IR_MOD] = "mod",
	[IR_AND_A] = "and", [IR_OR_A] = "or", [IR_XOR] = "xor",
	[IR_RS] = "rshift", [IR_LS] = "lshift",
	[IR_NEG] = "neg", [IR_NOT_A] = "not", [IR_NOT_L] = "lnot",
	[IR_EQ] = "eq", [IR_NE] = "ne", [IR_GT] = "gt", [IR_GE] = "ge",
	[IR_LT] = "lt", [IR_LE] = "le",
	[IR_LABEL] = "label", [IR_JMP] = "jmp", [IR_JZ] = "jz", [IR_JNZ] = "jnz",
	[IR_ARG] = "arg", [IR_CALL] = "call", [IR_RET] = "ret"
};

static void dump_extra (IR *ir, IRInstr *i)
{
	switch (i->op)
	{
	case IR_CONST:
		if (types[i->data_type].sign) printf(" %lld", (long long)i->imm64);
		else printf(" %llu", (unsigned long long)i->imm64);
		break;
	case IR_JMP: case IR_JZ: case IR_JNZ:
		printf(" -> L%u", i->target);
		break;
	case IR_PARAM: case IR_ARG:
		printf(" #%u", i->target);
		break;
	case IR_CALL:
		printf(" %.*s", (int)ir->fns[i->target].len, ir->fns[i->target].name);
		break;
	case IR_LD_GLOBAL: case IR_STR_GLOBAL:
	case IR_ADDR_GLOBAL:
		printf(" @%.*s", (int)ir->globals[i->target].len, ir->globals[i->target].name);
		break;
	case IR_STR_LIT:
		printf(" .LC%u", i->target);
		break;
	case IR_LD_PTR: case IR_STR_PTR: case IR_LD_LOCAL: case IR_STR_LOCAL:
		if (i->imm64) printf(" %+lld", (long long)i->imm64);
		break;
	default:
		break;
	}
}

static void dump_instr (IR *ir, IRInstr *i)
{
	if (i->op == IR_LABEL) {
		printf("  L%u:\n", i->target);
		return;
	}

	printf("    ");
	if (i->dst != NO_REG)
		printf("r%u = ", i->dst);
	printf("%s", op_names[i->op]);
	if (i->data_type != TYPE_VOID)
		printf(".%s", types[i->data_type].name);
	if (i->src1 != NO_REG)
		printf(" r%u", i->src1);
	if (i->src2 != NO_REG)
		printf(", r%u", i->src2);
	if (i->op == IR_SELECT)
		printf(", r%u", i->cond);
	dump_extra(ir, i);
	printf("\n");
}

static void dump_fn (IR *ir, uint32_t idx)
{
	IRFn *fn = ir->fns + idx;

	printf("%sfn %.*s(%u params, %u regs) : %s\n",
			fn->is_extern ? "extern " : "", (int)fn->len, fn->name,
			fn->param_count, fn->reg_count, types[fn->ret_type].name);
	for (uint32_t k = 0; k < fn->count; k++)
		dump_instr(ir, ir->instrs + fn->start + k);
	printf("\n");
}

void dump_ir (IR *ir)
{
	for (uint32_t i = 0; i < ir->global_count; i++)
		printf("%sglobal %.*s : %s\n",
				ir->globals[i].is_extern ? "extern " : "",
				(int)ir->globals[i].len, ir->globals[i].name,
				types[ir->globals[i].type].name);
	if (ir->global_count)
		printf("\n");

	for (uint32_t i = 0; i < ir->fn_count; i++)
		dump_fn(ir, i);
}
