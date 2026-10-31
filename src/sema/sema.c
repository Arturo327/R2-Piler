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

TypeId resolve_type_node (Sema *s, ASTNode *n)
{
	if (n->type_node == NO_TYPE_NODE) return n->data_type;

	uint32_t ref_idx = (uint32_t)(n - s->ast->nodes) + n->type_node;
	ASTNode *ref = s->ast->nodes + ref_idx;
	TypeId id = find_type(ref->str, ref->len);
	if (id == TYPE_ERROR)
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"unknown type '%.*s'", (int)ref->len, ref->str);
	n->type_node = NO_TYPE_NODE;
	n->data_type = id;
	return id;
}

void resolve_fn_types (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t param = s->ast->nodes[args].child;

	while (param != NO_NODE) {
		resolve_type_node(s, s->ast->nodes + param);
		param = s->ast->nodes[param].next_bro;
	}
	resolve_type_node(s, s->ast->nodes + ret);
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
		TypeId data_type, uint32_t decl_node)
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

uint32_t sema_wrap_cast (Sema *s, uint32_t child, TypeId to_type)
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
	cast.type_node = NO_TYPE_NODE;

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

void check_var_init_type (Sema *s, ASTNode *n, TypeId init_type)
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

static void decl_enum (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->len == 4 && memcmp(n->str, "main", 4) == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'main' must be a function");
		return;
	}
	if (symtab_find(&s->table, n->str, n->len) != NO_SYMBOL) {
		sema_declare(s, n->str, n->len, SYMBOL_ENUM, TYPE_ERROR, idx);
		return;
	}

	TypeId id = add_type(n->str, n->len, TYPE_KIND_ENUM, TYPE_u32);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return;
	}
	sema_declare(s, n->str, n->len, SYMBOL_ENUM, id, idx);
}

static void decl_enums (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		if (s->ast->nodes[idx].type == NODE_ENUM_DEC) decl_enum(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static void resolve_enum_base (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym;
	TypeId base;
	uint32_t loc;

	if (n->sym == NO_SYMBOL) return;
	sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR) return;

	loc = n->type_node != NO_TYPE_NODE
			? (uint32_t)(n - s->ast->nodes) + n->type_node : idx;
	base = resolve_type_node(s, n);
	if (base == TYPE_ERROR) {
		sym->type = TYPE_ERROR;
		return;
	}
	if (!type_is_integer(base)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + loc),
				"enum underlying type must be an integer type");
		sym->type = TYPE_ERROR;
		return;
	}
	types[sym->type].base = base;
	types[sym->type].size = types[base].size;
	types[sym->type].sign = types[base].sign;
}

static void resolve_enum_bases (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		if (s->ast->nodes[idx].type == NODE_ENUM_DEC) resolve_enum_base(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static uint64_t enum_max (TypeId id)
{
	unsigned bits = types[id].size * 8u;

	if (types[id].sign)
		return bits == 64 ? INT64_MAX : ((uint64_t)1 << (bits - 1)) - 1;
	return bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
}

static int enum_next_value (Sema *s, ASTNode *n, TypeId id, uint64_t prev,
		uint64_t *out)
{
	uint64_t max = enum_max(id);
	int full = types[id].sign ? (int64_t)prev >= (int64_t)max : prev >= max;

	if (full) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enum variant value overflows the underlying type %s",
				types[types[id].base].name);
		*out = 0;
		return 0;
	}
	*out = prev + 1;
	return 1;
}

static int variant_duplicate (Sema *s, const EnumVariant *vars, uint16_t count,
		ASTNode *n)
{
	for (uint16_t i = 0; i < count; i++)
		if (vars[i].len == n->len && memcmp(vars[i].name, n->str, n->len) == 0) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"duplicate enum variant '%.*s'", (int)n->len, n->str);
			return 1;
		}
	return 0;
}

static void bind_enum (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym;
	EnumVariant *vars;
	uint16_t count = 0;
	uint64_t prev = 0;
	uint32_t var;

	if (n->sym == NO_SYMBOL) return;
	sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR) return;

	for (var = n->child; var != NO_NODE; var = s->ast->nodes[var].next_bro)
		count++;
	if (count == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enum '%.*s' must have at least one variant",
				(int)n->len, n->str);
		return;
	}

	vars = arena_alloc(s->arena, sizeof(EnumVariant) * count);
	count = 0;
	for (var = n->child; var != NO_NODE; var = s->ast->nodes[var].next_bro) {
		ASTNode *v = s->ast->nodes + var;
		uint64_t value = 0;
		int ok = 1;

		variant_duplicate(s, vars, count, v);
		if (v->child != NO_NODE)
			ok = enum_const_value(s, v->child, sym->type, &value);
		else if (count != 0)
			ok = enum_next_value(s, v, sym->type, prev, &value);

		vars[count].name = v->str;
		vars[count].len = v->len;
		vars[count].value = ok ? value : 0;
		prev = vars[count].value;
		count++;
	}

	bind_enum_type(sym->type, vars, count);
}

static void bind_enums (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		if (s->ast->nodes[idx].type == NODE_ENUM_DEC) bind_enum(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static void check_root (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		if (n->type == NODE_VAR_DEC) check_global_var_init(s, idx);
		else if (n->type == NODE_FN_DEC) check_fn_dec(s, idx);
		else if (n->type != NODE_ENUM_DEC)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"only variable, function and enum declarations are allowed at the top level");
		idx = n->next_bro;
	}
}

static void decl_globals (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_FN_DEC) {
			uint32_t ret = s->ast->nodes[n->child].next_bro;
			resolve_fn_types(s, idx);
			sema_declare(s, n->str, n->len, SYMBOL_FN,
					s->ast->nodes[ret].data_type, idx);
		} else if (n->type == NODE_VAR_DEC) {
			resolve_type_node(s, n);
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

	decl_enums(s);
	resolve_enum_bases(s);
	decl_globals(s);
	bind_enums(s);

	check_root(s);
}

static const char *kind_name (uint8_t kind)
{
	switch (kind)
	{
	case SYMBOL_FN: return "fn";
	case SYMBOL_VAR: return "var";
	case SYMBOL_PARAM: return "param";
	case SYMBOL_ENUM: return "enum";
	default: return "unknown";
	}
}

static void dump_variants (TypeId id)
{
	const Type *t = types + id;

	for (uint16_t i = 0; i < t->variant_count; i++) {
		const EnumVariant *v = t->variants + i;

		if (t->sign)
			printf("  variant %.*s = %lld\n", (int)v->len, v->name,
					(long long)(int64_t)v->value);
		else printf("  variant %.*s = %llu\n", (int)v->len, v->name,
				(unsigned long long)v->value);
	}
}

void dump_symbols (SymbolTable *t)
{
	for (uint32_t i = 0; i < t->count; i++) {
		Symbol *sym = t->symbols + i;

		if (sym->kind == SYMBOL_ENUM && is_enum(sym->type)) {
			TypeId base = types[sym->type].base;

			printf("enum %.*s [%u:%u] depth=%u type=%s base=%s\n",
					(int)sym->len, sym->name, sym->line, sym->col,
					sym->depth, types[sym->type].name, types[base].name);
			dump_variants(sym->type);
			continue;
		}
		printf("%s %.*s [%u:%u] depth=%u type=%s\n",
				kind_name(sym->kind), (int)sym->len, sym->name,
				sym->line, sym->col, sym->depth, types[sym->type].name);
	}
}
