#ifndef AST_H
#define AST_H

#include <stdint.h>

#include "lexer/lexer.h"

#define NO_NODE 0xFFFFFFFF

typedef enum {
	NODE_ASSIGN, NODE_EQ, NODE_NE, NODE_GT, NODE_GE,
	NODE_LT, NODE_LE, NODE_AND_L, NODE_OR_L, NODE_NOT_L,	// Logic
	NODE_LIT_CHAR, NODE_LIT_STR, NODE_LIT_i64,		// Literals
	NODE_ADD, NODE_SUB, NODE_MUL, NODE_DIV,			// Arithmetic ops
	NODE_AND_A, NODE_OR_A, NODE_XOR, NODE_RS, NODE_LS,	// Bitwise ops
	NODE_NOT_A, NODE_NEG,					// Unary
	NODE_IF, NODE_WHILE, NODE_FOR,				// Branching
	NODE_FN_DEC, NODE_RET, NODE_ARGS_DECL, NODE_FN_CALL,	// Functions
	NODE_VAR_DEC, NODE_ID, NODE_BLOCK, NODE_ROOT		// misc
} NodeType;

typedef struct ASTNode {
	union {
		char *str;
		int64_t i64;
		char chr;
		uint8_t data_type;
	};

	uint32_t child;
	uint32_t next_bro;

	uint16_t line;
	uint16_t col;
	uint16_t len;

	uint8_t type;
} ASTNode;

typedef struct AST {
	ASTNode *nodes;
	uint32_t count;
	uint32_t cap;
} AST;

#endif
