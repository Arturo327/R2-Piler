#ifndef AST_H
#define AST_H

#include <stdint.h>

#include "type.h"

#define NO_NODE 0xFFFFFFFF
#define NO_TYPE_NODE 0
#define NODE_FLAG_STATIC 1
#define NODE_FLAG_UNION 2
#define NODE_FLAG_PACKED 4
#define NODE_FLAG_LITERAL 8
#define NODE_FLAG_BITFIELD 16
#define NODE_FLAG_BITFIELD_TYPED 32
#define NODE_FLAG_REF 64
#define NODE_FLAG_REF_LOWERED 128
#define NODE_FLAG_PAREN 1
#define NODE_FLAG_EXPORT 256
#define NODE_FLAG_PUB 512
#define NODE_FLAG_IMPORTED 8
#define NODE_FLAG_IMPORT_BODY 1024
#define NODE_FLAG_METHOD 2048
#define NODE_FLAG_PRIV 4096
#define NODE_FLAG_SELF 8192
#define NODE_FLAG_TEMP_ADDR 16384
#define NODE_FLAG_VARARG 1
#define NODE_FLAG_VARARG_TYPED 2
#define NODE_FLAG_VARARG_LOWERED 4
#define NODE_FLAG_INDIRECT 8
#define NODE_FLAG_CUSTOM_ALLOC 1

typedef enum {
	NODE_ERROR = 0, NODE_EMPTY,
	NODE_ASSIGN,
	NODE_ADD_ASSIGN, NODE_SUB_ASSIGN, NODE_MUL_ASSIGN, NODE_DIV_ASSIGN,	// op + assign
	NODE_MOD_ASSIGN, NODE_AND_A_ASSIGN, NODE_OR_A_ASSIGN, NODE_XOR_ASSIGN,
	NODE_LS_ASSIGN, NODE_RS_ASSIGN, NODE_INC, NODE_DEC,
	NODE_EQ, NODE_NE, NODE_GT, NODE_GE,
	NODE_LT, NODE_LE, NODE_AND_L, NODE_OR_L, NODE_NOT_L,	// Logic
	NODE_LIT_CHAR, NODE_LIT_STR, NODE_LIT_i64, NODE_LIT_u64,// Literals
	NODE_ADD, NODE_SUB, NODE_MUL, NODE_DIV,	NODE_MOD,	// Arithmetic ops
	NODE_AND_A, NODE_OR_A, NODE_XOR, NODE_RS, NODE_LS,	// Bitwise ops
	NODE_NOT_A, NODE_NEG, NODE_CAST, NODE_TERNARY,		// Unary, ternary
	NODE_IF, NODE_ELIF, NODE_ELSE, NODE_WHILE, NODE_FOR,	// Branching
	NODE_BREAK, NODE_CONTINUE,				// loop control
	NODE_SWITCH, NODE_CASE, NODE_DEFAULT,			// switch
	NODE_FN_DEC, NODE_ARGS_DEC, NODE_RET_DEC, NODE_RET, NODE_FN_CALL,// Functions
	NODE_VAR_DEC, NODE_ID, NODE_BLOCK, NODE_ROOT,		// misc
	NODE_ENUM_DEC, NODE_ENUM_VAL, NODE_DOT, NODE_TYPE_REF,
	NODE_TYPE_DEC, NODE_NEWTYPE_DEC, NODE_INDEX_INIT,
	NODE_EXTERN_FN_DEC, NODE_EXTERN_VAR_DEC,			// Extern (C interop)
	NODE_DEREF, NODE_ADDR, NODE_LIT_NULL, NODE_TYPE_PTR,
	NODE_TYPE_CONST, NODE_INDEX,
	NODE_ARR_DEC, NODE_ARRAY_INIT, NODE_TYPE_ARRAY, NODE_TYPE_SLICE,	// declarations
	NODE_STRUCT_DEC, NODE_FIELD_DEC, NODE_STRUCT_INIT, NODE_FIELD_INIT,	// struct
	NODE_RET_ITEM, NODE_MULTI_ASSIGN, NODE_NAMED_ARG,			// several
	NODE_ALIGNOF, NODE_OFFSETOF, NODE_SIZEOF, 				// layout queries
	NODE_SPREAD, NODE_MODULE_DEC,						// variadics & modules
	NODE_TYPE_FN, NODE_FN_PARAM,						// function pointer types
	NODE_IMPORT_DEC, NODE_FROM_DEC,						// imported modules
	NODE_DEPENDS_DEC,							// native library dependencies
	NODE_NEW, NODE_DELETE,							// heap
	NODE_CLASS_DEC,								// classes
	NODE_COUNT
} NodeType;

_Static_assert(NODE_COUNT <= 256, "ASTNode.type is uint8_t");

typedef struct ASTNode {
	union {
		char *str;
		int64_t i64;
		uint64_t u64;
		char chr;
	};

	uint32_t child;
	uint32_t next_bro;
	uint32_t sym;		// NODE_FIELD_DEC: bitfield width when NODE_FLAG_BITFIELD

	uint16_t type_node;
	uint16_t line;
	uint16_t col;
	uint16_t len;

	uint8_t type;
	uint16_t flags;
	TypeId data_type;
} ASTNode;

typedef struct AST {
	ASTNode *nodes;
	uint32_t count;
	uint32_t cap;
} AST;

static inline uint32_t ast_ret_count (const AST *ast, uint32_t ret)
{
	uint32_t count = 1;

	for (uint32_t item = ast->nodes[ret].child; item != NO_NODE;
			item = ast->nodes[item].next_bro)
		count++;
	return count;
}

static inline uint32_t ast_ret_at (const AST *ast, uint32_t ret, uint32_t i)
{
	uint32_t item = ast->nodes[ret].child;

	if (i == 0) return ret;
	while (i-- > 1 && item != NO_NODE)
		item = ast->nodes[item].next_bro;
	return item;
}

#endif
