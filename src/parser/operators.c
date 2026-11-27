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
