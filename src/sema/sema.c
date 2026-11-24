#include "sema/common.h"

#include <stdio.h>
#include <string.h>

#define RESERVED_PREFIX "__r2_"
#define RESERVED_PREFIX_LEN (sizeof(RESERVED_PREFIX) - 1)
#define RESERVED_MANGLE_PREFIX "_r2_"
#define RESERVED_MANGLE_LEN (sizeof(RESERVED_MANGLE_PREFIX) - 1)

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
	s->loop_depth = 0;
	s->init_node = NO_NODE;
	s->curr_ret = TYPE_VOID;
	s->curr_ret_list = NO_NODE;
	s->init_order_count = 0;
	s->init_order_cap = 16;
	s->init_order = arena_alloc(arena, sizeof(uint32_t) * s->init_order_cap);
	s->static_count = 0;
	s->static_cap = 16;
	s->statics = arena_alloc(arena, sizeof(uint32_t) * s->static_cap);
	s->dead_count = 0;
	s->dead_cap = 16;
	s->dead = arena_alloc(arena, sizeof(DeadWrite) * s->dead_cap);
	s->place_count = 0;
	s->place_cap = 16;
	s->places = arena_alloc(arena, sizeof(AggPlace) * s->place_cap);
	s->dead_place_count = 0;
	s->dead_place_cap = 16;
	s->dead_places = arena_alloc(arena, sizeof(AggPlace) * s->dead_place_cap);
	s->write_root = NO_NODE;
	s->addr_operand = 0;
	s->curr_fn = NO_SYMBOL;
	s->expr_depth = 0;
	s->too_deep = 0;
	s->ref_arg = 0;
	s->in_imported = 0;
	s->mod_name = NULL;
	s->mod_len = 0;
	s->mod_seen = 0;
	s->deps = NULL;
	s->dep_lens = NULL;
	s->dep_count = 0;
	s->dep_cap = 0;
	s->modules = NULL;
	s->module_count = 0;
	s->module_cap = 0;
	s->aliases = NULL;
	s->alias_count = 0;
	s->alias_cap = 0;
	s->bindings = NULL;
	s->binding_count = 0;
	s->binding_cap = 0;
	s->flag_stack = NULL;
	s->flag_cap = 0;
	s->flag_top = 0;

	symtab_init(&s->table, arena);
}

static const char *reserved_prefix (char *name, uint16_t len)
{
	if (len >= RESERVED_PREFIX_LEN && memcmp(name, RESERVED_PREFIX, RESERVED_PREFIX_LEN) == 0)
		return RESERVED_PREFIX;
	if (len >= RESERVED_MANGLE_LEN && memcmp(name, RESERVED_MANGLE_PREFIX, RESERVED_MANGLE_LEN) == 0)
		return RESERVED_MANGLE_PREFIX;
	return NULL;
}

static void check_reserved_name (Sema *s, char *name, uint16_t len, uint32_t decl_node)
{
	const char *prefix = s->depth == 0
			&& !(s->ast->nodes[decl_node].flags & NODE_FLAG_IMPORTED)
			? reserved_prefix(name, len) : NULL;

	if (!prefix) return;
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + decl_node),
			"identifiers starting with '%s' are reserved", prefix);
}

uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		TypeId data_type, uint32_t decl_node)
{
	check_reserved_name(s, name, len, decl_node);
	uint32_t prev = symtab_find(&s->table, name, len);

	if (prev != NO_SYMBOL && s->table.symbols[prev].depth == s->depth) {
		ASTNode *n = &s->ast->nodes[decl_node];
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' already declared in this scope", (int)len, name);
	}

	ASTNode *decl = s->ast->nodes + decl_node;
	Symbol sym = {
		.name = name,
		.len = len,
		.line = s->ast->nodes[decl_node].line,
		.col = s->ast->nodes[decl_node].col,
		.decl = decl_node,
		.depth = s->depth,
		.kind = (uint8_t)kind,
		.type = data_type,
		.view = kind == SYMBOL_VAR && decl->type == NODE_VAR_DEC
				&& type_kind(data_type) == TK_ARRAY
	};

	int aggregate = decl->data_type != TYPE_ERROR
			&& type_is_aggregate(decl->data_type);

	if (kind == SYMBOL_PARAM || (kind == SYMBOL_VAR && (decl->child != NO_NODE
			|| decl->type == NODE_EXTERN_VAR_DEC || aggregate
			|| (decl->flags & NODE_FLAG_IMPORTED))))
		sym.assigned = 1;

	uint32_t sym_idx = symtab_declare(&s->table, s->arena, sym);
	s->ast->nodes[decl_node].sym = sym_idx;
	agg_declare(s, sym_idx, decl_node);
	return sym_idx;
}

uint32_t sema_new_node (Sema *s, ASTNode node)
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

uint32_t sema_wrap_cast (Sema *s, uint32_t child, TypeId to_type)
{
	ASTNode *c = s->ast->nodes + child;
	ASTNode cast = {0};

	cast.type = NODE_CAST;
	cast.data_type = to_type;
	cast.line = c->line;
	cast.col = c->col;
	cast.len = 2;
	cast.child = child;
	cast.next_bro = c->next_bro;
	cast.sym = NO_NODE;
	cast.type_node = NO_TYPE_NODE;

	c->next_bro = NO_NODE;
	return sema_new_node(s, cast);
}

uint32_t sema_make_id (Sema *s, ASTNode *src)
{
	ASTNode raw = {0};

	raw.type = NODE_ID;
	raw.str = src->str;
	raw.len = src->len;
	raw.line = src->line;
	raw.col = src->col;
	raw.sym = NO_SYMBOL;
	raw.type_node = NO_TYPE_NODE;
	return sema_new_node(s, raw);
}

uint32_t sema_ref_value (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode raw = {0};

	raw.type = NODE_ID;
	raw.str = n->str;
	raw.len = n->len;
	raw.sym = n->sym;
	raw.data_type = n->data_type;
	raw.line = n->line;
	raw.col = n->col;
	raw.child = NO_NODE;
	raw.next_bro = NO_NODE;

	uint32_t raw_idx = sema_new_node(s, raw);
	n = s->ast->nodes + idx;
	TypeId ptr = n->data_type;

	n->type = NODE_DEREF;
	n->flags = NODE_FLAG_REF;
	n->child = raw_idx;
	n->sym = NO_SYMBOL;
	n->data_type = types[ptr].base;
	return n->data_type;
}

uint32_t sema_wrap_deref (Sema *s, uint32_t child, TypeId to_type)
{
	ASTNode *c = s->ast->nodes + child;
	ASTNode deref = {0};

	deref.type = NODE_DEREF;
	deref.data_type = to_type;
	deref.line = c->line;
	deref.col = c->col;
	deref.len = 1;
	deref.child = child;
	deref.next_bro = c->next_bro;
	deref.sym = NO_NODE;
	deref.type_node = NO_TYPE_NODE;

	c->next_bro = NO_NODE;
	return sema_new_node(s, deref);
}

uint32_t sema_wrap_addr (Sema *s, uint32_t child)
{
	ASTNode *c = s->ast->nodes + child;
	ASTNode addr = {0};

	addr.type = NODE_ADDR;
	addr.line = c->line;
	addr.col = c->col;
	addr.len = 1;
	addr.child = child;
	addr.next_bro = c->next_bro;
	addr.sym = NO_NODE;
	addr.type_node = NO_TYPE_NODE;

	c->next_bro = NO_NODE;
	return sema_new_node(s, addr);
}

void reserve_cast_nodes (Sema *s)
{
	uint32_t old_cap = s->ast->cap;
	uint32_t new_cap = s->ast->count * 2 + 64;

	if (new_cap <= old_cap) return;

	s->ast->nodes = arena_realloc(s->arena, s->ast->nodes,
			(size_t)old_cap * sizeof(ASTNode), (size_t)new_cap * sizeof(ASTNode));
	s->ast->cap = new_cap;
}

void check_const_init (Sema *s, ASTNode *n)
{
	if (!type_is_const(n->data_type) || n->child != NO_NODE) return;
	if (n->type == NODE_EXTERN_VAR_DEC || (n->flags & NODE_FLAG_IMPORTED)) return;
	error_report(s->err, ERR_ERROR, node_loc(n),
			"const %s '%.*s' requires an initializer",
			n->type == NODE_ARR_DEC ? "array" : "variable",
			(int)n->len, n->str);
}

void check_storage_decl (Sema *s, ASTNode *n)
{
	if (n->data_type == TYPE_ERROR) return;

	TypeKind kind = type_kind(n->data_type);
	int is_arr = n->type == NODE_ARR_DEC;
	int is_view = n->type == NODE_VAR_DEC && kind == TK_ARRAY;

	if (is_arr && kind != TK_ARRAY) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'arr' declarations must have an array type");
		n->data_type = TYPE_ERROR;
		return;
	}
	if (is_view && s->depth == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a view variable can only be declared inside a function;"
				" use 'arr' for a global array");
		n->data_type = TYPE_ERROR;
		return;
	}
	if (is_view && (n->flags & NODE_FLAG_STATIC)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a 'static' view is not supported; use 'static arr'");
		n->data_type = TYPE_ERROR;
	}
}

uint8_t *sema_flags_push (Sema *s, uint32_t n)
{
	if (s->flag_top + n > s->flag_cap) {
		uint32_t new_cap = s->flag_cap ? s->flag_cap << 1 : 1024;

		while (new_cap < s->flag_top + n) new_cap <<= 1;
		s->flag_stack = arena_realloc(s->arena, s->flag_stack,
				s->flag_cap, new_cap);
		s->flag_cap = new_cap;
	}
	uint8_t *p = s->flag_stack + s->flag_top;

	s->flag_top += n;
	return p;
}

void sema_flags_pop (Sema *s, uint32_t n)
{
	s->flag_top -= n;
}
