#ifndef AST_H
#define AST_H

#include <stdint.h>

#include "lexer/lexer.h"

typedef enum {
	NODE_ASSIGN, NODE_EQ, NODE_NE, NODE_GT, NODE_GE,
	NODE_LT, NODE_LE, NODE_AND_L, NODE_OR_L, NODE_NOT_L,	// Logic
	NODE_LIT_CHAR, NODE_LIT_STR, NODE_LIT_d64,		// Literals
	NODE_ADD, NODE_SUB, NODE_MUL, NODE_DIV,			// Arithmetic ops
	NODE_AND_A, NODE_OR_A, NODE_XOR, NODE_RS, NODE_LS,	// Bitwise ops
	NODE_IF, NODE_WHILE, NODE_FOR,				// Branching
	NODE_FN_DEC, NODE_RET, NODE_ARGS_DECL, NODE_FN_CALL	// Functions
	NODE_VAR_DEC, NODE_BLOCK, NODE_PROG			// misc
} NodeType;

typedef struct ASTNode {
	NodeType type;
	Token token;
	uint32_t child;
	uint32_t next_bro;
} ASTNode;

#endif
