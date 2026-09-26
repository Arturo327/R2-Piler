#include "sema/common.h"

#include <stdio.h>
#include <string.h>

#define RESERVED_PREFIX "__r2_"
#define RESERVED_PREFIX_LEN (sizeof(RESERVED_PREFIX) - 1)

ErrorLoc node_loc (ASTNode *n)
{
	ErrorLoc loc = {
		.line = n->line,
		.col = n->col,
		.len = n->len ? n->len : 1
	};
	return loc;
}

void sema_init (Sema *s, Arena *arena, AST *ast, ErrorReporter *err)
{
	s->ast = ast;
	s->err = err;
	s->arena = arena;
	s->depth = 0;
	s->init_node = NO_NODE;
	s->curr_ret = TYPE_VOID;
	s->init_order_count = 0;
	s->init_order_cap = 16;
	s->init_order = arena_alloc(arena, sizeof(uint32_t) * s->init_order_cap);
	s->expr_depth = 0;
	s->too_deep = 0;

	symtab_init(&s->table, arena);
}

static void check_reserved_name (Sema *s, char *name, uint16_t len, uint32_t decl_node)
{
	if (s->depth != 0 || len < RESERVED_PREFIX_LEN)
		return;
	if (memcmp(name, RESERVED_PREFIX, RESERVED_PREFIX_LEN) != 0)
		return;

	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + decl_node),
			"identifiers starting with '%s' are reserved", RESERVED_PREFIX);
}

uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		uint8_t data_type, uint32_t decl_node)
{
	check_reserved_name(s, name, len, decl_node);
	uint32_t prev = symtab_find(&s->table, name, len);

	if (prev != NO_SYMBOL && s->table.symbols[prev].depth == s->depth) {
		ASTNode *n = &s->ast->nodes[decl_node];
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' already declared in this scope", (int)len, name);
	}

	Symbol sym = {
		.name = name,
		.len = len,
		.line = s->ast->nodes[decl_node].line,
		.col = s->ast->nodes[decl_node].col,
		.decl = decl_node,
		.depth = s->depth,
		.kind = (uint8_t)kind,
		.type = data_type
	};

	if (kind == SYMBOL_PARAM || (kind == SYMBOL_VAR
				&& s->ast->nodes[decl_node].child != NO_NODE))
		sym.assigned = 1;

	uint32_t sym_idx = symtab_declare(&s->table, s->arena, sym);
	s->ast->nodes[decl_node].sym = sym_idx;
	return sym_idx;
}

static uint32_t sema_new_node (Sema *s, ASTNode node)
{
	uint32_t idx = s->ast->count++;

	if (idx >= s->ast->cap) {
		uint32_t new_cap = s->ast->cap << 1;
		s->ast->nodes = arena_realloc(s->arena, s->ast->nodes,
				(size_t)s->ast->cap * sizeof(ASTNode), (size_t)new_cap * sizeof(ASTNode));
		s->ast->cap = new_cap;
	}
	s->ast->nodes[idx] = node;
	return idx;
}

uint32_t sema_wrap_cast (Sema *s, uint32_t child, uint8_t to_type)
{
	ASTNode *c = &s->ast->nodes[child];
	ASTNode cast = {0};

	cast.type = NODE_CAST;
	cast.data_type = to_type;
	cast.line = c->line;
	cast.col = c->col;
	cast.len = 2;
	cast.child = child;
	cast.next_bro = c->next_bro;
	cast.sym = NO_NODE;

	c->next_bro = NO_NODE;
	return sema_new_node(s, cast);
}

static void reserve_cast_nodes (Sema *s)
{
	uint32_t old_cap = s->ast->cap;
	uint32_t new_cap = s->ast->count * 2 + 64;

	if (new_cap <= old_cap) return;

	s->ast->nodes = arena_realloc(s->arena, s->ast->nodes,
			(size_t)old_cap * sizeof(ASTNode), (size_t)new_cap * sizeof(ASTNode));
	s->ast->cap = new_cap;
}

void check_var_init_type (Sema *s, ASTNode *n, uint8_t init_type)
{
	int ok;

	if (init_type == TYPE_ERROR || n->data_type == TYPE_ERROR)
		return;
	if (init_type == n->data_type)
		return;

	n->child = force_cast(s, n->child, init_type, n->data_type, &ok);
	if (ok) return;

	const char *hint = init_type == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(n),
			"can't initialize '%.*s' (%s) with a value of type %s; %s",
			(int)n->len, n->str, types[n->data_type].name, types[init_type].name, hint);
}

static void record_init_order (Sema *s, uint32_t idx)
{
	if (s->init_order_count >= s->init_order_cap) {
		uint32_t old_cap = s->init_order_cap;
		s->init_order_cap <<= 1;
		s->init_order = arena_realloc(s->arena, s->init_order,
				old_cap * sizeof(uint32_t), s->init_order_cap * sizeof(uint32_t));
	}
	s->init_order[s->init_order_count++] = idx;
}

void check_global_var_init (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym = s->table.symbols + n->sym;
	uint32_t saved = s->init_node;

	if (n->child == NO_NODE) return;
	if (sym->state != INIT_PENDING) return;

	sym->state = INIT_CHECKING;
	s->init_node = idx;
	check_var_init_type(s, n, check_expr(s, n->child));
	s->init_node = saved;
	sym->state = INIT_DONE;
	record_init_order(s, idx);
}

static void check_root (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		if (n->type == NODE_VAR_DEC) check_global_var_init(s, idx);
		else if (n->type == NODE_FN_DEC) check_fn_dec(s, idx);
		else error_report(s->err, ERR_ERROR, node_loc(n),
				"only variable and function declarations are allowed at the top level");
		idx = n->next_bro;
	}
}

static void decl_globals (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_FN_DEC) {
			uint32_t args = n->child;
			uint32_t ret = s->ast->nodes[args].next_bro;
			uint8_t data_type = s->ast->nodes[ret].data_type;
			sema_declare(s, n->str, n->len, SYMBOL_FN, data_type, idx);
		} else if (n->type == NODE_VAR_DEC) {
			if (n->len == 4 && memcmp(n->str, "main", 4) == 0)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"'main' must be a function");
			sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);
		}

		idx = n->next_bro;
	}
}

void sema_run (Sema *s)
{
	reserve_cast_nodes(s);
	decl_globals(s);
	check_root(s);
}

static const char *kind_name (uint8_t kind)
{
	switch (kind)
	{
	case SYMBOL_FN: return "fn";
	case SYMBOL_VAR: return "var";
	case SYMBOL_PARAM: return "param";
	default: return "unknown";
	}
}

void dump_symbols (SymbolTable *t)
{
	for (uint32_t i = 0; i < t->count; i++) {
		Symbol *sym = t->symbols + i;
		printf("%s %.*s [%u:%u] depth=%u type=%s\n",
				kind_name(sym->kind), (int)sym->len, sym->name,
				sym->line, sym->col, sym->depth, types[sym->type].name);
	}
}
