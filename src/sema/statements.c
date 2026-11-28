#include "sema/common.h"

#include <string.h>

#define MAX_PARAMS 255

static void check_statement (Sema *s, uint32_t idx);
static int stmt_returns (Sema *s, uint32_t idx);
static int stmt_terminates (Sema *s, uint32_t idx);
static int jump_exits_loop (Sema *s, uint32_t idx, uint32_t depth);

static void check_block_body (Sema *s, uint32_t idx)
{
	uint32_t stmt = s->ast->nodes[idx].child;
	int dead = 0;

	while (stmt != NO_NODE) {
		uint32_t next = s->ast->nodes[stmt].next_bro;

		if (dead) {
			error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + stmt),
					"code will never be executed");
			dead = 0;
		}
		check_statement(s, stmt);
		if (next != NO_NODE && stmt_terminates(s, stmt)) dead = 1;
		stmt = next;
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
	if (sym->decl != NO_NODE
			&& (s->ast->nodes[sym->decl].flags & NODE_FLAG_SELF)) return;

	if (sym->read) {
		warn_dead_writes(s, idx, sym);
		return;
	}
	if (sym->name[0] == '_') return;

	ErrorLoc loc = { .line = sym->line, .col = sym->col, .len = sym->len };

	error_report(s->err, ERR_WARNING, loc, "unused %s '%.*s'",
			sym->kind == SYMBOL_PARAM ? "parameter" : "variable",
			(int)sym->len, sym->name);
}

static void pop_scope (Sema *s, uint32_t mark)
{
	for (uint32_t i = mark; i < s->table.act_count; i++) {
		uint32_t sym_idx = s->table.active[i];
		Symbol *sym = s->table.symbols + sym_idx;

		warn_unused(s, sym_idx, sym);
		sema_pop_places(s, sym_idx, sym);
	}

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

static void check_self_params (Sema *s, uint32_t idx)
{
	if (s->ast->nodes[idx].flags & NODE_FLAG_METHOD) return;
	uint32_t p = s->ast->nodes[s->ast->nodes[idx].child].child;
	while (p != NO_NODE) {
		if (s->ast->nodes[p].flags & NODE_FLAG_SELF)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p), "'self' is only valid as the first parameter of a method");
		p = s->ast->nodes[p].next_bro;
	}
}

static uint32_t declare_params (Sema *s, uint32_t args)
{
	uint32_t param = s->ast->nodes[args].child;
	uint32_t count = 0;
	while (param != NO_NODE) {
		ASTNode *p = s->ast->nodes + param;
		int by_ref = (p->flags & NODE_FLAG_REF_LOWERED) != 0;
		uint32_t next = p->next_bro;
		uint32_t sym = sema_declare(s, param, SYMBOL_PARAM, p->data_type);

		s->table.symbols[sym].by_ref = by_ref;
		count++;
		param = next;
	}
	return count;
}

void check_fn_dec (Sema *s, uint32_t idx)
{
	resolve_fn_types(s, idx);
	if (s->ast->nodes[idx].flags & NODE_FLAG_IMPORTED) {
		check_signature_types(s, idx, 0);
		check_export_fn(s, idx);
		return;
	}
	check_signature_types(s, idx, 0);
	check_export_fn(s, idx);
	check_self_params(s, idx);
	if (!(s->ast->nodes[idx].flags & NODE_FLAG_METHOD))
		check_main_signature(s, idx);
	uint32_t args = s->ast->nodes[idx].child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t body = s->ast->nodes[ret].next_bro;

	uint32_t mark = s->table.act_count;
	s->depth++;
	s->curr_ret = s->ast->nodes[ret].data_type;
	s->curr_ret_list = ret;
	uint32_t saved_fn = s->curr_fn;

	s->curr_fn = s->ast->nodes[idx].sym;
	uint32_t count = declare_params(s, args);
	if (count > MAX_PARAMS)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx), "too many parameters (max %d)", MAX_PARAMS);
	check_block_body(s, body);
	s->curr_fn = saved_fn;

	if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR
			&& !stmt_returns(s, body))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx), "function may reach the end without returning");
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

int check_cond (Sema *s, uint32_t cond, int alw_err)
{
	TypeId type = check_expr(s, cond);

	type = sema_truth_value(s, cond, type);
	if (((type == TYPE_VOID && s->ast->nodes[cond].type != NODE_EMPTY) || type == TYPE_STR)
			|| (type != TYPE_ERROR && type_is_aggregate(type)))
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

		sema_clear_pending(s);
		if (n->type == NODE_ELIF) {
			body = s->ast->nodes[n->child].next_bro;
			check_cond(s, n->child, 1);
		} else {
			has_else = 1;
		}
		check_body(s, body);

		if (!stmt_returns(s, body))
			symtab_intersect_flags(&s->table, acc, mark);
		sema_load_flags(s, snap, mark);
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
	uint8_t *snap = sema_flags_push(s, (size_t)mark * 2);
	uint8_t *acc = snap + mark;

	memset(acc, 1, mark);
	if (cv == 1) {
		check_body(s, body);
		symtab_save_flags(&s->table, snap, mark);
		check_elif_chain(s, s->ast->nodes[body].next_bro, snap, acc, mark);
		sema_load_flags(s, snap, mark);
		sema_flags_pop(s, (size_t)mark * 2);
		return;
	}

	symtab_save_flags(&s->table, snap, mark);
	sema_clear_pending(s);
	check_body(s, body);
	if (!stmt_returns(s, body))
		symtab_save_flags(&s->table, acc, mark);
	sema_load_flags(s, snap, mark);

	if (!check_elif_chain(s, s->ast->nodes[body].next_bro, snap, acc, mark))
		symtab_intersect_flags(&s->table, acc, mark);
	sema_load_flags(s, acc, mark);
	sema_flags_pop(s, (size_t)mark * 2);
}

static void check_loop_jump (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	const char *kw = n->type == NODE_BREAK ? "break" : "continue";
	uint64_t level = n->u64;

	n->data_type = TYPE_VOID;
	if (level == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%s 0' is invalid; the loop level must be at least 1", kw);
		return;
	}
	if (level <= s->loop_depth) return;

	if (s->loop_depth == 0)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%s' is only valid inside a loop", kw);
	else error_report(s->err, ERR_ERROR, node_loc(n),
			"'%s %llu' exceeds the %u enclosing loop%s", kw,
			(unsigned long long)level, s->loop_depth,
			s->loop_depth == 1 ? "" : "s");
}

static void check_loop_else (Sema *s, uint32_t els, uint32_t body, uint8_t *snap, uint32_t mark)
{
	if (els == NO_NODE) return;

	sema_clear_pending(s);
	check_body(s, s->ast->nodes[els].child);
	if (jump_exits_loop(s, body, 1)) {
		symtab_intersect_flags(&s->table, snap, mark);
		sema_load_flags(s, snap, mark);
	}
}

static void check_while (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;
	uint32_t els = s->ast->nodes[body].next_bro;

	check_cond(s, cond, 0);
	sema_clear_pending(s);

	uint32_t mark = s->table.act_count;
	uint8_t *snap = sema_flags_push(s, (size_t)mark);

	symtab_save_flags(&s->table, snap, mark);
	s->loop_depth++;
	check_body(s, body);
	s->loop_depth--;
	symtab_intersect_flags(&s->table, snap, mark);
	sema_load_flags(s, snap, mark);
	check_loop_else(s, els, body, snap, mark);
	sema_flags_pop(s, (size_t)mark);
}

static void check_for (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t init = n->child;
	uint32_t cond = s->ast->nodes[init].next_bro;
	uint32_t updt = s->ast->nodes[cond].next_bro;
	uint32_t body = s->ast->nodes[updt].next_bro;
	uint32_t els = s->ast->nodes[body].next_bro;

	uint32_t mark = s->table.act_count;
	s->depth++;

	check_statement(s, init);
	check_cond(s, cond, 0);
	sema_clear_pending(s);

	uint32_t body_mark = s->table.act_count;
	uint8_t *snap = sema_flags_push(s, (size_t)body_mark);

	symtab_save_flags(&s->table, snap, body_mark);
	s->loop_depth++;
	check_body(s, body);
	s->loop_depth--;
	s->place_count = 0;
	uint8_t kind = s->ast->nodes[updt].type;

	if (kind == NODE_INC || kind == NODE_DEC) check_inc_dec(s, updt);
	else if (kind == NODE_BLOCK) check_statement(s, updt);
	else finalize_type(s, updt, check_expr(s, updt));

	symtab_intersect_flags(&s->table, snap, body_mark);
	sema_load_flags(s, snap, body_mark);
	check_loop_else(s, els, body, snap, body_mark);
	sema_flags_pop(s, (size_t)body_mark);

	s->depth--;
	pop_scope(s, mark);
}

static uint32_t switch_value_count (Sema *s, uint32_t cond)
{
	uint32_t count = 0;
	uint32_t clause = s->ast->nodes[cond].next_bro;

	while (clause != NO_NODE) {
		ASTNode *c = s->ast->nodes + clause;
		uint32_t v = c->child;

		while (v != NO_NODE && s->ast->nodes[v].type != NODE_BLOCK) {
			count++;
			v = s->ast->nodes[v].next_bro;
		}
		clause = c->next_bro;
	}
	return count;
}

static uint32_t case_body (Sema *s, uint32_t clause)
{
	uint32_t v = s->ast->nodes[clause].child;

	while (v != NO_NODE && s->ast->nodes[v].type != NODE_BLOCK)
		v = s->ast->nodes[v].next_bro;
	return v;
}

static uint32_t check_case_value (Sema *s, uint32_t pos, TypeId type, uint64_t *seen, uint32_t *nseen)
{
	TypeId vt = check_expr(s, pos);
	ASTNode *n = s->ast->nodes + pos;

	if (vt == TYPE_ERROR) return pos;

	uint64_t bits;
	if (!lit_node_bits(s, n, &bits)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"case value must be a constant expression");
		return pos;
	}

	int ok;
	uint32_t cast = force_cast(s, pos, vt, type, &ok);

	if (!ok) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + pos),
				"case value of type %s does not match the switch type %s;"
				" use an explicit cast with 'as'",
				type_display_name(vt), types[type].name);
		return pos;
	}
	n = s->ast->nodes + cast;
	if (!lit_node_bits(s, n, &bits)) return cast;

	for (uint32_t i = 0; i < *nseen; i++) {
		if (seen[i] == bits) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"duplicate case value");
			return cast;
		}
	}
	seen[(*nseen)++] = bits;
	return cast;
}

static void check_case_clause (Sema *s, uint32_t clause, TypeId type, uint64_t *seen, uint32_t *nseen)
{
	uint32_t v = s->ast->nodes[clause].child;
	uint32_t prev = NO_NODE;

	if (v == NO_NODE || s->ast->nodes[v].type == NODE_BLOCK) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + clause),
				"case requires at least one value");
		return;
	}
	while (v != NO_NODE && s->ast->nodes[v].type != NODE_BLOCK) {
		uint32_t next = s->ast->nodes[v].next_bro;
		uint32_t nv = check_case_value(s, v, type, seen, nseen);

		if (nv != v) {
			if (prev == NO_NODE) s->ast->nodes[clause].child = nv;
			else s->ast->nodes[prev].next_bro = nv;
			s->ast->nodes[nv].next_bro = next;
		}
		prev = nv;
		v = next;
	}
}

static void check_switch_body (Sema *s, uint32_t body, uint8_t *snap, uint8_t *acc, uint32_t mark)
{
	sema_clear_pending(s);
	check_body(s, body);
	if (!stmt_returns(s, body))
		symtab_intersect_flags(&s->table, acc, mark);
	sema_load_flags(s, snap, mark);
}

static void check_switch_clauses (Sema *s, uint32_t idx, TypeId type)
{
	uint32_t cond = s->ast->nodes[idx].child;
	uint32_t mark = s->table.act_count;
	uint8_t *snap = sema_flags_push(s, (size_t)mark * 2);
	uint8_t *acc = snap + mark;
	uint64_t *seen = arena_alloc(s->arena, sizeof(uint64_t) * (switch_value_count(s, cond) + 1));
	uint32_t nseen = 0;
	uint32_t clause = s->ast->nodes[cond].next_bro;
	int has_default = 0;

	memset(acc, 1, mark);
	symtab_save_flags(&s->table, snap, mark);

	while (clause != NO_NODE) {
		ASTNode *c = s->ast->nodes + clause;
		uint32_t body;

		if (c->type == NODE_DEFAULT) {
			if (has_default)
				error_report(s->err, ERR_ERROR, node_loc(c), "duplicate default clause");
			has_default = 1;
			body = c->child;
		} else {
			check_case_clause(s, clause, type, seen, &nseen);
			body = case_body(s, clause);
		}
		check_switch_body(s, body, snap, acc, mark);
		clause = s->ast->nodes[clause].next_bro;
	}

	if (!has_default)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"switch requires a 'default' clause");
	sema_load_flags(s, acc, mark);
	sema_flags_pop(s, (size_t)mark * 2);
}

static void check_switch (Sema *s, uint32_t idx)
{
	uint32_t cond = s->ast->nodes[idx].child;
	TypeId type = finalize_type(s, cond, check_expr(s, cond));

	TypeKind kind = type_kind(type_repr(type));
	if (type != TYPE_ERROR && !(kind == TK_INT || kind == TK_ENUM)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cond),
				"expression of type %s cannot be switched on",
				type_display_name(type));
		type = TYPE_ERROR;
	}
	check_switch_clauses(s, idx, type);
}

static uint32_t ret_value_type (Sema *s, uint32_t idx, uint32_t val, TypeId ret)
{
	TypeId val_type = check_expr(s, val);

	if (val_type == TYPE_ERROR || ret == TYPE_ERROR || val_type == ret)
		return val;
	if (sema_array_slice_compat(val_type, ret)) return val;

	int ok;
	uint32_t cast = force_cast(s, val, val_type, ret, &ok);
	if (ok) return cast;

	const char *hint = val_type == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"cannot return %s from a function returning %s; %s",
			type_display_name(val_type), types[ret].name, hint);
	return cast;
}

static void check_bare_return (Sema *s, uint32_t idx, uint32_t count)
{
	ASTNode *n = s->ast->nodes + idx;

	if (count > 1)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"missing return values: this function returns %u values", count);
	else if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"missing return value of type %s", types[s->curr_ret].name);
}

static void check_single_return (Sema *s, uint32_t idx, TypeId ret)
{
	uint32_t val = s->ast->nodes[idx].child;

	if (ret == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"function returning void cannot return a value");
		check_expr(s, val);
		return;
	}

	uint32_t cast = ret_value_type(s, idx, val, ret);

	s->ast->nodes[idx].child = cast;
	warn_stack_return(s, cast, ret);
}

static TypeId ret_type_at (Sema *s, uint32_t list, uint32_t i)
{
	if (list == NO_NODE) return i == 0 ? s->curr_ret : TYPE_ERROR;
	return s->ast->nodes[ast_ret_at(s->ast, list, i)].data_type;
}

static void check_multi_return (Sema *s, uint32_t idx, uint32_t list, uint32_t count)
{
	uint32_t val = s->ast->nodes[idx].child;
	uint32_t prev = NO_NODE;
	uint32_t i = 0;

	while (val != NO_NODE) {
		uint32_t next = s->ast->nodes[val].next_bro;

		if (i < count) {
			TypeId rt = ret_type_at(s, list, i);
			uint32_t cast = ret_value_type(s, idx, val, rt);

			if (prev == NO_NODE) s->ast->nodes[idx].child = cast;
			else s->ast->nodes[prev].next_bro = cast;
			val = cast;
			warn_stack_return(s, val, rt);
		} else {
			check_expr(s, val);
		}
		prev = val;
		val = next;
		i++;
	}

	if (i == count) return;
	if (count == 1) error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"this function returns 1 value, but %u were returned", i);
	else error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"this function returns %u values, but %u were returned", count, i);
}

static void check_return (Sema *s, uint32_t idx)
{
	uint32_t list = s->curr_ret_list;
	uint32_t count = list == NO_NODE ? 1 : ast_ret_count(s->ast, list);
	uint32_t first = s->ast->nodes[idx].child;

	if (first == NO_NODE) {
		check_bare_return(s, idx, count);
		return;
	}
	if (count == 1 && s->ast->nodes[first].next_bro == NO_NODE) {
		check_single_return(s, idx, s->curr_ret);
		return;
	}
	check_multi_return(s, idx, list, count);
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

static int switch_returns (Sema *s, uint32_t idx)
{
	uint32_t cond = s->ast->nodes[idx].child;
	uint32_t clause = s->ast->nodes[cond].next_bro;
	int has_default = 0;

	while (clause != NO_NODE) {
		ASTNode *c = s->ast->nodes + clause;
		uint32_t body = c->type == NODE_DEFAULT ? c->child : case_body(s, clause);

		if (!stmt_returns(s, body)) return 0;
		if (c->type == NODE_DEFAULT) has_default = 1;
		clause = c->next_bro;
	}
	return has_default;
}

static uint32_t loop_body_index (Sema *s, uint32_t idx)
{
	uint32_t body = s->ast->nodes[idx].child;

	if (s->ast->nodes[idx].type == NODE_WHILE)
		return s->ast->nodes[body].next_bro;
	for (int k = 0; k < 3; k++)
		body = s->ast->nodes[body].next_bro;
	return body;
}

static uint32_t loop_else_index (Sema *s, uint32_t idx)
{
	uint32_t body = loop_body_index(s, idx);
	return s->ast->nodes[body].next_bro;
}

static int jump_exits_loop (Sema *s, uint32_t idx, uint32_t depth)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->type == NODE_BREAK)
		return n->u64 >= depth;
	if (n->type == NODE_WHILE || n->type == NODE_FOR) {
		uint32_t body = loop_body_index(s, idx);
		if (jump_exits_loop(s, body, depth + 1)) return 1;
		uint32_t els = s->ast->nodes[body].next_bro;
		return els != NO_NODE && jump_exits_loop(s, s->ast->nodes[els].child, depth);
	}

	for (uint32_t c = n->child; c != NO_NODE; c = s->ast->nodes[c].next_bro)
		if (jump_exits_loop(s, c, depth)) return 1;
	return 0;
}

static int loop_else_returns (Sema *s, uint32_t idx)
{
	uint32_t els = loop_else_index(s, idx);
	return els != NO_NODE && stmt_returns(s, s->ast->nodes[els].child);
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
	case NODE_SWITCH: return switch_returns(s, idx);
	case NODE_WHILE:
		return !jump_exits_loop(s, loop_body_index(s, idx), 1)
				&& (const_cond_value(s, n->child) == 1
				|| loop_else_returns(s, idx));
	case NODE_FOR:
		cond = s->ast->nodes[n->child].next_bro;
		return !jump_exits_loop(s, loop_body_index(s, idx), 1)
				&& (s->ast->nodes[cond].type == NODE_EMPTY
				|| const_cond_value(s, cond) == 1
				|| loop_else_returns(s, idx));
	default: return 0;
	}
}

static int stmt_terminates (Sema *s, uint32_t idx)
{
	uint8_t type = s->ast->nodes[idx].type;

	return type == NODE_BREAK || type == NODE_CONTINUE || stmt_returns(s, idx);
}

static void warn_no_effect (Sema *s, uint32_t idx, TypeId type)
{
	if (type == TYPE_ERROR || expr_has_effect(s, idx)) return;

	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"expression has no effect; its result is discarded");
}

static int reject_nested_decl (Sema *s, ASTNode *n)
{
	const char *what = NULL;

	switch (n->type)
	{
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_FN_DEC:
		if (n->flags & NODE_FLAG_EXPORT)
			what = "'export' is only valid on top-level declarations";
		else if (n->flags & NODE_FLAG_PUB)
			what = "'pub' is only valid on top-level declarations";
		else if (n->type == NODE_FN_DEC)
			what = "functions can only be declared at the top level";
		break;
	case NODE_ENUM_DEC: what = "enums can only be declared at the top level"; break;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		what = "types can only be declared at the top level"; break;
	case NODE_STRUCT_DEC: what = "structs can only be declared at the top level"; break;
	case NODE_CLASS_DEC: what = "classes can only be declared at the top level"; break;
	case NODE_MODULE_DEC: what = "'module' can only be declared at the top level"; break;
	case NODE_DEPENDS_DEC: what = "'depends' can only be declared at the top level"; break;
	case NODE_EXTERN_FN_DEC: case NODE_EXTERN_VAR_DEC:
		what = "extern declarations can only appear at the top level"; break;
	default: return 0;
	}
	if (!what) return 0;
	error_report(s->err, ERR_ERROR, node_loc(n), "%s", what);
	return 1;
}

static void check_statement (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_VAR_DEC: case NODE_ARR_DEC:
		reject_nested_decl(s, n);
		check_var_dec(s, idx);
		return;
	case NODE_BLOCK: check_block(s, idx); return;
	case NODE_RET: check_return(s, idx); return;
	case NODE_BREAK: case NODE_CONTINUE: check_loop_jump(s, idx); return;
	case NODE_MULTI_ASSIGN: check_multi_assign(s, idx); return;
	case NODE_WHILE: check_while(s, idx); return;
	case NODE_FOR: check_for(s, idx); return;
	case NODE_IF: check_if(s, idx); return;
	case NODE_SWITCH: check_switch(s, idx); return;
	case NODE_INC: case NODE_DEC: check_inc_dec(s, idx); return;
	case NODE_DELETE: check_delete(s, idx); return;
	case NODE_DROP: check_drop(s, idx); return;
	case NODE_RESET: check_reset(s, idx); return;
	case NODE_EMPTY: n->data_type = TYPE_VOID; return;
	case NODE_FN_DEC: case NODE_ENUM_DEC:
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
	case NODE_STRUCT_DEC: case NODE_CLASS_DEC:
	case NODE_MODULE_DEC: case NODE_DEPENDS_DEC:
	case NODE_EXTERN_FN_DEC: case NODE_EXTERN_VAR_DEC:
		reject_nested_decl(s, n);
		return;
	default: {
		TypeId type = check_expr(s, idx);
		finalize_type(s, idx, type);
		warn_no_effect(s, idx, type);
		return;
	}
	}
}
