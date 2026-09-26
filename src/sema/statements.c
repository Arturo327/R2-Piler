#include "sema/common.h"

#include <string.h>

#define MAX_PARAMS 255

static void check_statement (Sema *s, uint32_t idx);
static int stmt_returns (Sema *s, uint32_t idx);

static void check_block_body (Sema *s, uint32_t idx)
{
	uint32_t stmt = s->ast->nodes[idx].child;
	while (stmt != NO_NODE) {
		check_statement(s, stmt);
		stmt = s->ast->nodes[stmt].next_bro;
	}
}

static void check_block (Sema *s, uint32_t idx)
{
	uint32_t mark = s->table.act_count;
	s->depth++;

	check_block_body(s, idx);

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_var_dec (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->child != NO_NODE)
		check_var_init_type(s, n, check_expr(s, n->child));

	sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);
}

static void check_main_signature (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	int bad_params = s->ast->nodes[args].child != NO_NODE;
	int bad_ret = s->ast->nodes[ret].data_type != TYPE_i64;

	if (n->len != 4 || memcmp(n->str, "main", 4) != 0)
		return;
	if (bad_params || bad_ret)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'main' must be declared as 'fn main() : i64'");
}

void check_fn_dec (Sema *s, uint32_t idx)
{
	check_main_signature(s, idx);
	uint32_t args = s->ast->nodes[idx].child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t body = s->ast->nodes[ret].next_bro;

	uint32_t mark = s->table.act_count;
	s->depth++;
	s->curr_ret = s->ast->nodes[ret].data_type;

	uint32_t param = s->ast->nodes[args].child;
	uint32_t count = 0;
	while (param != NO_NODE) {
		ASTNode *p = s->ast->nodes + param;
		sema_declare(s, p->str, p->len, SYMBOL_PARAM, p->data_type, param);
		count++;
		param = p->next_bro;
	}
	if (count > MAX_PARAMS)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"too many parameters (max %d)", MAX_PARAMS);

	check_block_body(s, body);

	if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR
			&& !stmt_returns(s, body))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"function may reach the end without returning");

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_scoped_statement (Sema *s, uint32_t idx)
{
	uint32_t mark = s->table.act_count;
	s->depth++;

	check_statement(s, idx);

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_body (Sema *s, uint32_t idx)
{
	if (s->ast->nodes[idx].type == NODE_BLOCK) {
		check_statement(s, idx);
		return;
	}

	check_scoped_statement(s, idx);
}

static void check_elif_chain (Sema *s, uint32_t idx)
{
	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_ELIF) {
			uint32_t cond = n->child;
			uint32_t body = s->ast->nodes[cond].next_bro;
			uint8_t type = check_expr(s, cond);
			if (type == TYPE_VOID)
				error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cond),
						"expression with resulting type void is not valid as a condition");
			finalize_type(s, cond, type);
			check_body(s, body);
		} else {
			check_body(s, n->child);
		}

		idx = n->next_bro;
	}
}

static void check_if (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;

	uint8_t type = check_expr(s, cond);
	if (type == TYPE_VOID)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cond),
				"expression with resulting type void is not valid as a condition");
	finalize_type(s, cond, type);

	check_body(s, body);
	check_elif_chain(s, s->ast->nodes[body].next_bro);
}

static void check_while (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;

	uint8_t type = check_expr(s, cond);
	if (type == TYPE_VOID)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cond),
				"expression with resulting type void is not valid as a condition");
	finalize_type(s, cond, type);

	check_body(s, body);
}

static void check_for (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t init = n->child;
	uint32_t cond = s->ast->nodes[init].next_bro;
	uint32_t updt = s->ast->nodes[cond].next_bro;
	uint32_t body = s->ast->nodes[updt].next_bro;

	uint32_t mark = s->table.act_count;
	s->depth++;

	check_statement(s, init);
	uint8_t cond_type = check_expr(s, cond);
	if (cond_type == TYPE_VOID && s->ast->nodes[cond].type != NODE_EMPTY)
		error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[cond]),
				"expression with resulting type void is not valid as a condition");

	finalize_type(s, cond, cond_type);
	check_body(s, body);
	finalize_type(s, updt, check_expr(s, updt));

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_return (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint8_t val_type;
	int ok;

	if (n->child == NO_NODE) {
		if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"missing return value of type %s",
					types[s->curr_ret].name);
		return;
	}

	if (s->curr_ret == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"function returning void cannot return a value");
		check_expr(s, n->child);
		return;
	}

	val_type = check_expr(s, n->child);
	if (val_type == TYPE_ERROR || s->curr_ret == TYPE_ERROR || val_type == s->curr_ret)
		return;

	n->child = force_cast(s, n->child, val_type, s->curr_ret, &ok);
	if (ok) return;

	const char *hint = val_type == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot return %s from a function returning %s; %s",
			types[val_type].name, types[s->curr_ret].name, hint);
}

static int block_returns (Sema *s, uint32_t idx)
{
	uint32_t stmt = s->ast->nodes[idx].child;
	while (stmt != NO_NODE) {
		if (stmt_returns(s, stmt)) return 1;
		stmt = s->ast->nodes[stmt].next_bro;
	}
	return 0;
}

static int if_returns (Sema *s, uint32_t idx)
{
	uint32_t then_body = s->ast->nodes[s->ast->nodes[idx].child].next_bro;
	uint32_t branch = s->ast->nodes[then_body].next_bro;
	int has_else = 0;

	if (!stmt_returns(s, then_body)) return 0;

	while (branch != NO_NODE) {
		ASTNode *b = s->ast->nodes + branch;
		uint32_t body = (b->type == NODE_ELIF)
				? s->ast->nodes[b->child].next_bro : b->child;
		if (!stmt_returns(s, body)) return 0;
		if (b->type == NODE_ELSE) has_else = 1;
		branch = b->next_bro;
	}
	return has_else;
}

static int stmt_returns (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_RET: return 1;
	case NODE_BLOCK: return block_returns(s, idx);
	case NODE_IF: return if_returns(s, idx);
	default: return 0;
	}
}

static void check_statement (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_VAR_DEC: check_var_dec(s, idx); return;
	case NODE_BLOCK: check_block(s, idx); return;
	case NODE_RET: check_return(s, idx); return;
	case NODE_WHILE: check_while(s, idx); return;
	case NODE_FOR: check_for(s, idx); return;
	case NODE_IF: check_if(s, idx); return;
	case NODE_EMPTY: n->data_type = TYPE_VOID; return;
	case NODE_FN_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"functions can only be declared at the top level");
		return;
	default:
		finalize_type(s, idx, check_expr(s, idx));
		return;
	}
}
