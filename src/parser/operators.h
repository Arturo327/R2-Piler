#ifndef PARSER_OPERATORS_H
#define PARSER_OPERATORS_H

#include <stdint.h>

typedef enum {
	OP_NONE = 0,
	OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
	OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
	OP_NEG, OP_NOT, OP_BNOT,
	OP_ASSIGN, OP_ADD_ASSIGN, OP_SUB_ASSIGN, OP_MUL_ASSIGN, OP_DIV_ASSIGN,
	OP_MOD_ASSIGN, OP_AND_ASSIGN, OP_OR_ASSIGN, OP_XOR_ASSIGN,
	OP_SHL_ASSIGN, OP_SHR_ASSIGN,
	OP_INC, OP_DEC,
	OP_INDEX,
	OP_COUNT
} OpId;

_Static_assert(OP_COUNT <= 256, "ASTNode.op is uint8_t");

const char *op_canon (uint8_t op);
const char *op_spelling (uint8_t op);

#endif