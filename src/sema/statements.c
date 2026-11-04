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

static void warn_dead_write (Sema *s, Symbol *sym, uint32_t node)
{
	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + node),
			"value assigned to '%.*s' is not used", (int)sym->len, sym->name);
}

static void warn_dead_writes (Sema *s, uint32_t idx, Symbol *sym)
{
	for (uint32_t i = 0; i < s->dead_count; i++)
		if (s->dead[i].sym == idx)
			warn_dead_write(s, sym, s->dead[i].node);

	if (sym->pending_write && !sym->addressed)
		warn_dead_write(s, sym, sym->write_node);
}

static void warn_unused (Sema *s, uint32_t idx, Symbol *sym)
{
	if (sym->kind != SYMBOL_VAR && sym->kind != SYMBOL_PARAM) return;

	if (sym->read) {
		warn_dead_writes(s, idx, sym);
		return;
	}

	ErrorLoc loc = { .line = sym->line, .col = sym->col, .len = sym->len };

	error_report(s->err, ERR_WARNING, loc, "unused %s '%.*s'",
			sym->kind == SYMBOL_PARAM ? "parameter" : "variable",
			(int)sym->len, sym->name);
}

static void pop_scope (Sema *s, uint32_t mark)
{
	for (uint32_t i = mark; i < s->table.act_count; i++)
		warn_unused(s, s->table.active[i],
				&s->table.symbols[s->table.active[i]]);

	symtab_pop_scope(&s->table, mark);
}

static void check_block (Sema *s, uint32_t idx)
{
	uint32_t mark = s->table.act_count;
	s->depth++;

	check_block_body(s, idx);

	s->depth--;
	pop_scope(s, mark);
}

static void check_var_dec (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	int has_init = n->child != NO_NODE;
	int is_null = has_init && is_null_value(s, n->child);

	resolve_type_node(s, n);
	n = s->ast->nodes + idx;
	check_storage_decl(s, n);

	if (has_init) {
		n = s->ast->nodes + idx;
		TypeKind kind = n->data_type == TYPE_ERROR
				? TK_ERROR : type_kind(n->data_type);

		if (kind == TK_ARRAY) {
			check_array_init(s, idx);
		} else if (s->ast->nodes[n->child].type == NODE_ARRAY_INIT) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + n->child),
					"'{...}' initializer requires an array type");
		} else {
			check_var_init_type(s, n, check_expr(s, n->child));
		}
	}

	n = s->ast->nodes + idx;
	uint32_t sym = sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);

	if (is_null) s->table.symbols[sym].null_known = 1;
	if (has_init) sym_write(s, sym, idx);
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

static void check_signature_types (Sema *s, uint32_t idx, int is_extern)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	TypeId ret_type = s->ast->nodes[ret].data_type;

	if (ret_type != TYPE_ERROR
			&& (type_kind(ret_type) == TK_ARRAY || type_kind(ret_type) == TK_SLICE))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"functions cannot return an array or a slice");

	uint32_t p = s->ast->nodes[args].child;
	while (p != NO_NODE) {
		TypeId type = s->ast->nodes[p].data_type;

		if (type != TYPE_ERROR && is_extern && type_kind(type) == TK_SLICE)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"extern functions cannot take slice parameters");

		p = s->ast->nodes[p].next_bro;
	}
}

void check_fn_dec (Sema *s, uint32_t idx)
{
	resolve_fn_types(s, idx);
	check_signature_types(s, idx, 0);
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
	pop_scope(s, mark);
}

void check_extern_fn (Sema *s, uint32_t idx)
{
	resolve_fn_types(s, idx);
	check_signature_types(s, idx, 1);
	ASTNode *n = s->ast->nodes + idx;
	uint32_t count = 0;

	if (n->len == 4 && memcmp(n->str, "main", 4) == 0)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'main' cannot be declared extern");

	for (uint32_t param = s->ast->nodes[n->child].child; param != NO_NODE;
			param = s->ast->nodes[param].next_bro)
		count++;
	if (count > MAX_PARAMS)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"too many parameters (max %d)", MAX_PARAMS);
}

static void check_scoped_statement (Sema *s, uint32_t idx)
{
	uint32_t mark = s->table.act_count;
	s->depth++;

	check_statement(s, idx);

	s->depth--;
	pop_scope(s, mark);
}

static void check_body (Sema *s, uint32_t idx)
{
	if (s->ast->nodes[idx].type == NODE_BLOCK) {
		check_statement(s, idx);
		return;
	}

	check_scoped_statement(s, idx);
}

static int const_cond_value (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->type == NODE_CAST)
		return const_cond_value(s, n->child);

	switch (n->type)
	{
	case NODE_LIT_i64: return n->i64 != 0 ? 1 : 0;
	case NODE_LIT_u64: return n->u64 != 0 ? 1 : 0;
	case NODE_LIT_CHAR: return n->chr != 0 ? 1 : 0;
	case NODE_LIT_NULL: return 0;
	default: return -1;
	}
}

static int check_cond (Sema *s, uint32_t cond, int alw_err)
{
	TypeId type = check_expr(s, cond);

	if (((type == TYPE_VOID && s->ast->nodes[cond].type != NODE_EMPTY) || type == TYPE_STR)
			|| (type != TYPE_ERROR && (type_kind(type) == TK_ARRAY
			|| type_kind(type) == TK_SLICE)))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cond),
				"expression with resulting type %s is not valid as a condition",
				types[type].name);
	finalize_type(s, cond, type);

	int v = const_cond_value(s, cond);
	if (v == 0 || (alw_err && v > 0)) error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + cond),
			"condition is always %s", v ? "true" : "false");
	return v;
}

static int check_elif_chain (Sema *s, uint32_t idx, uint8_t *snap, uint8_t *acc, uint32_t mark)
{
	int has_else = 0;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		uint32_t body = n->child;

		symtab_clear_pending(&s->table);
		if (n->type == NODE_ELIF) {
			body = s->ast->nodes[n->child].next_bro;
			check_cond(s, n->child, 1);
		} else {
			has_else = 1;
		}
		check_body(s, body);

		if (!stmt_returns(s, body))
			symtab_intersect_flags(&s->table, acc, mark);
		symtab_load_flags(&s->table, snap, mark);
		idx = n->next_bro;
	}

	return has_else;
}

static void check_if (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;
	int cv = check_cond(s, cond, 1);

	uint32_t mark = s->table.act_count;
	uint8_t *snap = arena_alloc(s->arena, (size_t)mark * 2);
	uint8_t *acc = snap + mark;

	memset(acc, 1, mark);
	if (cv == 1) {
		check_body(s, body);
		symtab_save_flags(&s->table, snap, mark);
		check_elif_chain(s, s->ast->nodes[body].next_bro, snap, acc, mark);
		symtab_load_flags(&s->table, snap, mark);
		return;
	}

	symtab_save_flags(&s->table, snap, mark);
	symtab_clear_pending(&s->table);
	check_body(s, body);
	if (!stmt_returns(s, body))
		symtab_save_flags(&s->table, acc, mark);
	symtab_load_flags(&s->table, snap, mark);

	if (!check_elif_chain(s, s->ast->nodes[body].next_bro, snap, acc, mark))
		symtab_intersect_flags(&s->table, acc, mark);
	symtab_load_flags(&s->table, acc, mark);
}

static void check_while (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;

	check_cond(s, cond, 0);
	symtab_clear_pending(&s->table);

	uint32_t mark = s->table.act_count;
	uint8_t *snap = arena_alloc(s->arena, (size_t)mark);

	symtab_save_flags(&s->table, snap, mark);
	check_body(s, body);
	symtab_intersect_flags(&s->table, snap, mark);
	symtab_load_flags(&s->table, snap, mark);
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
	check_cond(s, cond, 0);
	symtab_clear_pending(&s->table);

	uint32_t body_mark = s->table.act_count;
	uint8_t *snap = arena_alloc(s->arena, (size_t)body_mark);

	symtab_save_flags(&s->table, snap, body_mark);
	check_body(s, body);
	if (s->ast->nodes[updt].type == NODE_INC || s->ast->nodes[updt].type == NODE_DEC)
		check_inc_dec(s, updt);
	else finalize_type(s, updt, check_expr(s, updt));
	symtab_intersect_flags(&s->table, snap, body_mark);
	symtab_load_flags(&s->table, snap, body_mark);

	s->depth--;
	pop_scope(s, mark);
}

static void check_return (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

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

	TypeId val_type = check_expr(s, n->child);
	if (val_type == TYPE_ERROR || s->curr_ret == TYPE_ERROR || val_type == s->curr_ret)
		return;

	int ok;
	n->child = force_cast(s, n->child, val_type, s->curr_ret, &ok);
	if (ok) return;

	const char *hint = val_type == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot return %s from a function returning %s; %s",
			type_display_name(val_type), types[s->curr_ret].name, hint);
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
	uint32_t cond;

	switch (n->type)
	{
	case NODE_RET: return 1;
	case NODE_BLOCK: return block_returns(s, idx);
	case NODE_IF: return if_returns(s, idx);
	case NODE_WHILE: return const_cond_value(s, n->child) == 1;
	case NODE_FOR:
		cond = s->ast->nodes[n->child].next_bro;
		return s->ast->nodes[cond].type == NODE_EMPTY
				|| const_cond_value(s, cond) == 1;
	default: return 0;
	}
}

static void warn_no_effect (Sema *s, uint32_t idx, TypeId type)
{
	if (type == TYPE_ERROR || expr_has_effect(s, idx)) return;

	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"expression has no effect; its result is discarded");
}

static void check_statement (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_VAR_DEC: case NODE_ARR_DEC: check_var_dec(s, idx); return;
	case NODE_BLOCK: check_block(s, idx); return;
	case NODE_RET: check_return(s, idx); return;
	case NODE_WHILE: check_while(s, idx); return;
	case NODE_FOR: check_for(s, idx); return;
	case NODE_IF: check_if(s, idx); return;
	case NODE_INC: case NODE_DEC: check_inc_dec(s, idx); return;
	case NODE_EMPTY: n->data_type = TYPE_VOID; return;
	case NODE_FN_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"functions can only be declared at the top level");
		return;
	case NODE_ENUM_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enums can only be declared at the top level");
		return;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"types can only be declared at the top level");
		return;
	case NODE_EXTERN_FN_DEC: case NODE_EXTERN_VAR_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"extern declarations can only appear at the top level");
		return;
	default: {
		TypeId type = check_expr(s, idx);
		finalize_type(s, idx, type);
		warn_no_effect(s, idx, type);
		return;
	}
	}
}
