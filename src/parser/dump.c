#include "parser/common.h"

#include <stdio.h>

static const char *node_names[NODE_COUNT] = {
	[NODE_ERROR] = "NODE_ERROR",
	[NODE_EMPTY] = "NODE_EMPTY",
	[NODE_ASSIGN] = "NODE_ASSIGN",
	[NODE_ADD_ASSIGN] = "NODE_ADD_ASSIGN",
	[NODE_SUB_ASSIGN] = "NODE_SUB_ASSIGN",
	[NODE_MUL_ASSIGN] = "NODE_MUL_ASSIGN",
	[NODE_DIV_ASSIGN] = "NODE_DIV_ASSIGN",
	[NODE_MOD_ASSIGN] = "NODE_MOD_ASSIGN",
	[NODE_AND_A_ASSIGN] = "NODE_AND_A_ASSIGN",
	[NODE_OR_A_ASSIGN] = "NODE_OR_A_ASSIGN",
	[NODE_XOR_ASSIGN] = "NODE_XOR_ASSIGN",
	[NODE_LS_ASSIGN] = "NODE_LS_ASSIGN",
	[NODE_RS_ASSIGN] = "NODE_RS_ASSIGN",
	[NODE_INC] = "NODE_INC",
	[NODE_DEC] = "NODE_DEC",
	[NODE_EQ] = "NODE_EQ",
	[NODE_NE] = "NODE_NE",
	[NODE_GT] = "NODE_GT",
	[NODE_GE] = "NODE_GE",
	[NODE_LT] = "NODE_LT",
	[NODE_LE] = "NODE_LE",
	[NODE_AND_L] = "NODE_AND_L",
	[NODE_OR_L] = "NODE_OR_L",
	[NODE_NOT_L] = "NODE_NOT_L",
	[NODE_LIT_CHAR] = "NODE_LIT_CHAR",
	[NODE_LIT_STR] = "NODE_LIT_STR",
	[NODE_LIT_i64] = "NODE_LIT_i64",
	[NODE_LIT_u64] = "NODE_LIT_u64",
	[NODE_ADD] = "NODE_ADD",
	[NODE_SUB] = "NODE_SUB",
	[NODE_MUL] = "NODE_MUL",
	[NODE_DIV] = "NODE_DIV",
	[NODE_MOD] = "NODE_MOD",
	[NODE_AND_A] = "NODE_AND_A",
	[NODE_OR_A] = "NODE_OR_A",
	[NODE_XOR] = "NODE_XOR",
	[NODE_RS] = "NODE_RS",
	[NODE_LS] = "NODE_LS",
	[NODE_NOT_A] = "NODE_NOT_A",
	[NODE_NEG] = "NODE_NEG",
	[NODE_CAST] = "NODE_CAST",
	[NODE_IF] = "NODE_IF",
	[NODE_ELIF] = "NODE_ELIF",
	[NODE_ELSE] = "NODE_ELSE",
	[NODE_WHILE] = "NODE_WHILE",
	[NODE_FOR] = "NODE_FOR",
	[NODE_FN_DEC] = "NODE_FN_DEC",
	[NODE_ARGS_DEC] = "NODE_ARGS_DEC",
	[NODE_RET_DEC] = "NODE_RET_DEC",
	[NODE_RET] = "NODE_RET",
	[NODE_FN_CALL] = "NODE_FN_CALL",
	[NODE_VAR_DEC] = "NODE_VAR_DEC",
	[NODE_ID] = "NODE_ID",
	[NODE_BLOCK] = "NODE_BLOCK",
	[NODE_ROOT] = "NODE_ROOT",
	[NODE_ENUM_DEC] = "NODE_ENUM_DEC",
	[NODE_ENUM_VAL] = "NODE_ENUM_VAL",
	[NODE_DOT] = "NODE_DOT",
	[NODE_TYPE_REF] = "NODE_TYPE_REF",
	[NODE_TYPE_DEC] = "NODE_TYPE_DEC",
	[NODE_NEWTYPE_DEC] = "NODE_NEWTYPE_DEC",
	[NODE_EXTERN_FN_DEC] = "NODE_EXTERN_FN_DEC",
	[NODE_EXTERN_VAR_DEC] = "NODE_EXTERN_VAR_DEC",
	[NODE_DEREF] = "NODE_DEREF",
	[NODE_ADDR] = "NODE_ADDR",
	[NODE_SIZEOF] = "NODE_SIZEOF",
	[NODE_LIT_NULL] = "NODE_LIT_NULL",
	[NODE_TYPE_PTR] = "NODE_TYPE_PTR",
	[NODE_INDEX] = "NODE_INDEX",
	[NODE_ARR_DEC] = "NODE_ARR_DEC",
	[NODE_ARRAY_INIT] = "NODE_ARRAY_INIT",
	[NODE_TYPE_ARRAY] = "NODE_TYPE_ARRAY",
	[NODE_TYPE_SLICE] = "NODE_TYPE_SLICE",
};

static inline void indent (int depth)
{
	while (depth-- > 0) printf("  ");
}

static void dump_node_value (ASTNode *n)
{
	switch (n->type)
	{
	case NODE_LIT_i64:
		printf(" i64=%llu", (unsigned long long)n->u64);
		break;
	case NODE_LIT_u64:
		printf(" u64=%llu", (unsigned long long)n->u64);
		break;
	case NODE_LIT_CHAR:
		printf(" char='");
		print_escaped(&n->chr, 1, '\'');
		printf("'");
		break;
	case NODE_LIT_STR: case NODE_ID:
		printf(" str=\"");
		print_escaped(n->str, n->len, '"');
		printf("\"");
		break;
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_FN_DEC: case NODE_FN_CALL:
	case NODE_ENUM_DEC: case NODE_ENUM_VAL:
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
	case NODE_EXTERN_FN_DEC: case NODE_EXTERN_VAR_DEC:
		if (n->str) printf(" name=\"%.*s\"", (int)n->len, n->str);
		break;
	case NODE_DOT:
		printf(" field=\"%.*s\"", (int)n->len, n->str);
		break;
	default:
		break;
	}
}

static void dump_dim (AST *ast, uint32_t idx)
{
	if (idx == NO_NODE) {
		printf("?");
		return;
	}

	ASTNode *n = &ast->nodes[idx];

	switch (n->type)
	{
	case NODE_LIT_i64: printf("%lld", (long long)n->i64); break;
	case NODE_LIT_u64: printf("%llu", (unsigned long long)n->u64); break;
	case NODE_LIT_CHAR: printf("%d", (int)n->chr); break;
	default: printf("?"); break;
	}
}

static void dump_type_name (AST *ast, ASTNode *n)
{
	if (n->type == NODE_TYPE_PTR) {
		printf("ptr ");
		if (n->type_node != NO_TYPE_NODE) {
			uint32_t idx = (uint32_t)(n - ast->nodes) + n->type_node;
			dump_type_name(ast, &ast->nodes[idx]);
		} else {
			printf("%s", types[n->data_type].name);
		}
		return;
	}
	if (n->type == NODE_TYPE_ARRAY || n->type == NODE_TYPE_SLICE) {
		if (n->type_node != NO_TYPE_NODE) {
			uint32_t idx = (uint32_t)(n - ast->nodes) + n->type_node;
			dump_type_name(ast, &ast->nodes[idx]);
		} else {
			printf("%s", types[n->data_type].name);
		}
		if (n->type == NODE_TYPE_SLICE) printf("[]");
		else {
			printf("[");
			dump_dim(ast, n->child);
			printf("]");
		}
		return;
	}
	if (n->type == NODE_TYPE_REF) {
		printf("%.*s", (int)n->len, n->str);
		return;
	}
	printf("%s", types[n->data_type].name);
}

static void dump_type (AST *ast, ASTNode *n)
{
	if (n->type_node != NO_TYPE_NODE) {
		uint32_t idx = (uint32_t)(n - ast->nodes) + n->type_node;
		printf(" type=");
		dump_type_name(ast, &ast->nodes[idx]);
		return;
	}
	printf(" type=%s", types[n->data_type].name);
}

static void dump_node (AST *ast, uint32_t idx, int depth)
{
	while (idx != NO_NODE) {
		ASTNode *n = &ast->nodes[idx];
		uint32_t child = n->child;
		uint32_t next = n->next_bro;
		const char *name = n->type < NODE_COUNT ? node_names[n->type] : "UNKNOWN";

		indent(depth);
		printf("%s [%u:%u]", name, n->line, n->col);

		dump_node_value(n);

		if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC
				|| n->type == NODE_EXTERN_VAR_DEC || n->type == NODE_RET_DEC
				|| n->type == NODE_CAST || n->type == NODE_ENUM_DEC || n->type == NODE_TYPE_DEC
				|| n->type == NODE_NEWTYPE_DEC || (n->type == NODE_SIZEOF && n->child == NO_NODE))
			dump_type(ast, n);

		printf("\n");
		dump_node(ast, child, depth + 1);
		idx = next;
	}
}

void dump_ast (AST *ast)
{
	if (ast->count == 0)
		return;
	dump_node(ast, 0, 0);
}
