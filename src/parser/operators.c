#include "parser/operators.h"

typedef struct {
	const char *canon;
	const char *spelling;
} OpName;

static const OpName op_names[OP_COUNT] = {
	[OP_EQ] = { "eq", "==" },
	[OP_NE] = { "ne", "!=" },
	[OP_LT] = { "lt", "<" },
	[OP_LE] = { "le", "<=" },
	[OP_GT] = { "gt", ">" },
	[OP_GE] = { "ge", ">=" },
	[OP_ADD] = { "add", "+" },
	[OP_SUB] = { "sub", "-" },
	[OP_MUL] = { "mul", "*" },
	[OP_DIV] = { "div", "/" },
	[OP_MOD] = { "mod", "%" },
	[OP_AND] = { "and", "&" },
	[OP_OR] = { "or", "|" },
	[OP_XOR] = { "xor", "^" },
	[OP_SHL] = { "shl", "<<" },
	[OP_SHR] = { "shr", ">>" },
	[OP_NEG] = { "neg", "-" },
	[OP_NOT] = { "not", "!" },
	[OP_BNOT] = { "bnot", "~" },
	[OP_ASSIGN] = { "assign", "=" },
	[OP_ADD_ASSIGN] = { "add_assign", "+=" },
	[OP_SUB_ASSIGN] = { "sub_assign", "-=" },
	[OP_MUL_ASSIGN] = { "mul_assign", "*=" },
	[OP_DIV_ASSIGN] = { "div_assign", "/=" },
	[OP_MOD_ASSIGN] = { "mod_assign", "%=" },
	[OP_AND_ASSIGN] = { "and_assign", "&=" },
	[OP_OR_ASSIGN] = { "or_assign", "|=" },
	[OP_XOR_ASSIGN] = { "xor_assign", "^=" },
	[OP_SHL_ASSIGN] = { "shl_assign", "<<=" },
	[OP_SHR_ASSIGN] = { "shr_assign", ">>=" },
	[OP_INC] = { "inc", "++" },
	[OP_DEC] = { "dec", "--" },
	[OP_INDEX] = { "index", "[]" }
};

const char *op_canon (uint8_t op)
{
	if (op >= OP_COUNT || !op_names[op].canon) return "";
	return op_names[op].canon;
}

const char *op_spelling (uint8_t op)
{
	if (op >= OP_COUNT || !op_names[op].spelling) return "";
	return op_names[op].spelling;
}