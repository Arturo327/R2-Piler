#include "sema/common.h"
#include "sema/expr_internal.h"

#define MAX_EXPR_DEPTH 5000

const uint8_t compound_base[NODE_COUNT] = {
	[NODE_ADD_ASSIGN] = NODE_ADD, [NODE_SUB_ASSIGN] = NODE_SUB,
	[NODE_MUL_ASSIGN] = NODE_MUL, [NODE_DIV_ASSIGN] = NODE_DIV,
	[NODE_MOD_ASSIGN] = NODE_MOD, [NODE_AND_A_ASSIGN] = NODE_AND_A,
	[NODE_OR_A_ASSIGN] = NODE_OR_A, [NODE_XOR_ASSIGN] = NODE_XOR,
	[NODE_LS_ASSIGN] = NODE_LS, [NODE_RS_ASSIGN] = NODE_RS
};

static TypeId check_expr_node (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64:
	case NODE_LIT_CHAR: case NODE_LIT_STR:
		return check_literal(s, idx);
	case NODE_ID: return check_id(s, idx);
	case NODE_LIT_NULL: n->data_type = TYPE_NULL; return TYPE_NULL;
	case NODE_DEREF: return check_deref(s, idx);
	case NODE_INDEX: return check_index(s, idx);
	case NODE_ADDR: return check_addr(s, idx);
	case NODE_SIZEOF: return check_sizeof(s, idx);
	case NODE_ALIGNOF: return check_alignof(s, idx);
	case NODE_OFFSETOF: return check_offsetof(s, idx);
	case NODE_DOT: return check_dot(s, idx);
	case NODE_FN_CALL: return check_fn_call(s, idx);
	case NODE_ASSIGN: return check_assign(s, idx);
	case NODE_ADD_ASSIGN: case NODE_SUB_ASSIGN: case NODE_MUL_ASSIGN:
	case NODE_DIV_ASSIGN: case NODE_MOD_ASSIGN: case NODE_AND_A_ASSIGN:
	case NODE_OR_A_ASSIGN: case NODE_XOR_ASSIGN: case NODE_LS_ASSIGN:
	case NODE_RS_ASSIGN: return check_compound_assign(s, idx);
	case NODE_INC: case NODE_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"increment/decrement cannot be used as an expression");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	case NODE_NEG: case NODE_NOT_L: case NODE_NOT_A:
		return check_unary(s, idx);
	case NODE_CAST: return check_cast(s, idx);
	case NODE_TERNARY: return check_ternary(s, idx);
	case NODE_EMPTY: n->data_type = TYPE_VOID; return TYPE_VOID;
	case NODE_ARRAY_INIT: case NODE_STRUCT_INIT:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a '{...}' literal needs an explicit type; write '{...} as T'");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	case NODE_ERROR: n->data_type=TYPE_ERROR; return TYPE_ERROR;
	default: return check_binary(s, idx);
	}
}

TypeId check_expr (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (s->expr_depth >= MAX_EXPR_DEPTH) {
		if (!s->too_deep)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"expression or initializer chain nested too deeply");
		s->too_deep = 1;
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	s->expr_depth++;
	TypeId type = check_expr_node(s, idx);
	s->expr_depth--;
	return type;
}

int expr_has_effect (Sema *s, uint32_t idx)
{
	if (idx == NO_NODE) return 0;
	ASTNode *n = s->ast->nodes + idx;
	uint32_t child = n->child;

	switch (n->type)
	{
	case NODE_FN_CALL: case NODE_INC: case NODE_DEC:
	case NODE_ASSIGN: return 1;
	case NODE_LIT_i64: case NODE_LIT_u64: case NODE_LIT_CHAR:
	case NODE_LIT_STR: case NODE_LIT_NULL: case NODE_EMPTY:
		return 0;
	case NODE_ID: case NODE_DEREF: case NODE_ADDR:
	case NODE_NEG: case NODE_NOT_L: case NODE_NOT_A:
	case NODE_SIZEOF: case NODE_CAST:
		return expr_has_effect(s, child);
	case NODE_ALIGNOF: case NODE_OFFSETOF:
		return 0;
	case NODE_INDEX:
		return expr_has_effect(s, child)
				|| expr_has_effect(s, s->ast->nodes[child].next_bro);
	case NODE_ARRAY_INIT: case NODE_STRUCT_INIT:
		for (uint32_t k = child; k != NO_NODE; k = s->ast->nodes[k].next_bro) {
			if (expr_has_effect(s, k)) return 1;
		}
		return 0;
	case NODE_TERNARY: {
		uint32_t then_idx = s->ast->nodes[child].next_bro;

		return expr_has_effect(s, child) || expr_has_effect(s, then_idx)
				|| expr_has_effect(s, s->ast->nodes[then_idx].next_bro);
	}
	default:
		break;
	}

	if (compound_base[n->type] != 0 || child == NO_NODE) return 1;
	return expr_has_effect(s, child)
			|| expr_has_effect(s, s->ast->nodes[child].next_bro);
}
