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

static int ptr_base_ok (TypeId base)
{
	TypeKind kind = type_kind(base);

	return kind == TK_VOID || kind == TK_INT || kind == TK_ENUM || kind == TK_FN
			|| kind == TK_NEWTYPE || kind == TK_PTR || kind == TK_STRUCT;
}

static TypeId resolve_type_ref (Sema *s, ASTNode *ref, uint8_t *done, int *deferred)
{
	TypeId id = sema_lookup_type(s, ref->str, ref->len);

	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"unknown type '%.*s'", (int)ref->len, ref->str);
		return TYPE_ERROR;
	}
	if (done) {
		if (type_kind(id) == TK_ALIAS && !done[id]) {
			*deferred = 1;
			return TYPE_ERROR;
		}
		if (type_kind(id) == TK_ALIAS) id = type_canon(id);
		return id;
	}

	id = type_canon(id);
	if (id == TYPE_ERROR || types[id].base == TYPE_ERROR)
		return TYPE_ERROR;
	return id;
}

static uint64_t trunc_dim (uint64_t v, TypeId type)
{
	if (type == TYPE_ERROR || !types[type].size) return v;
	uint32_t bits = (uint32_t)types[type].size * 8u;
	if (bits >= 64) return v;

	uint64_t mask = ((uint64_t)1 << bits) - 1;
	v &= mask;
	if (types[type].sign && (v >> (bits - 1)) & 1) v |= ~mask;
	return v;
}

int dim_value (Sema *s, uint32_t idx, uint64_t *out);

static int dim_unary (Sema *s, ASTNode *n, uint64_t *out)
{
	uint64_t a;
	if (!dim_value(s, n->child, &a)) return 0;
	if (n->type == NODE_CAST) *out = trunc_dim(a, n->data_type);
	else if (n->type == NODE_NEG) *out = 0 - a;
	else *out = ~a;
	return 1;
}

static int dim_binary (Sema *s, ASTNode *n, uint64_t *out)
{
	uint32_t rhs = s->ast->nodes[n->child].next_bro;
	if (rhs == NO_NODE) return 0;
	uint64_t a;
	uint64_t b;
	if (!dim_value(s, n->child, &a) || !dim_value(s, rhs, &b)) return 0;

	switch (n->type)
	{
	case NODE_ADD: *out = a + b; return 1;
	case NODE_SUB: *out = a - b; return 1;
	case NODE_MUL: *out = a * b; return 1;
	case NODE_DIV: if (!b) return 0; *out = a / b; return 1;
	case NODE_MOD: if (!b) return 0; *out = a % b; return 1;
	case NODE_AND_A: *out = a & b; return 1;
	case NODE_OR_A: *out = a | b; return 1;
	case NODE_XOR: *out = a ^ b; return 1;
	case NODE_LS: if (b >= 64) return 0; *out = a << b; return 1;
	case NODE_RS: if (b >= 64) return 0; *out = a >> b; return 1;
	default: return 0;
	}
}

int dim_value (Sema *s, uint32_t idx, uint64_t *out)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64: *out = n->u64; return 1;
	case NODE_LIT_CHAR: *out = (uint64_t)(int64_t)n->chr; return 1;
	case NODE_CAST: case NODE_NEG: case NODE_NOT_A:
		return n->child != NO_NODE && dim_unary(s, n, out);
	default:
		return n->child != NO_NODE && dim_binary(s, n, out);
	}
}

static TypeId resolve_array_spec (Sema *s, ASTNode *ref, uint8_t *done, int *deferred, int slice)
{
	TypeId id = resolve_type_spec(s, ref, done, deferred);

	if (deferred && *deferred) return TYPE_ERROR;
	if (id == TYPE_ERROR) return TYPE_ERROR;

	if (!types[id].size) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"%s element type cannot be void", slice ? "slice" : "array");
		return TYPE_ERROR;
	}
	if (type_count >= TYPE_MAX) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
		return TYPE_ERROR;
	}
	if (slice) return type_slice_of(id);

	uint64_t count;
	if (!dim_value(s, ref->child, &count)) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"array size must be a constant integer expression");
		return TYPE_ERROR;
	}
	if (!count) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"array size must be greater than zero");
		return TYPE_ERROR;
	}
	if (count > UINT32_MAX) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "array is too large");
		return TYPE_ERROR;
	}

	id = type_array_of(id, (uint32_t)count);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "array is too large");
		return TYPE_ERROR;
	}
	return id;
}

static TypeId resolve_fn_item (Sema *s, uint32_t *item, TypeId *out,
		uint8_t *done, int *deferred, const char *what)
{
	if (*item == NO_NODE) return TYPE_ERROR;

	ASTNode *p = s->ast->nodes + *item;
	TypeId id = resolve_type_spec(s, p, done, deferred);

	*item = p->next_bro;
	if (deferred && *deferred) return TYPE_ERROR;
	if (id == TYPE_ERROR) return TYPE_ERROR;
	if (id == TYPE_VOID || type_kind(id) == TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(p),
				"invalid function pointer %s type '%s'", what, types[id].name);
		return TYPE_ERROR;
	}
	*out = id;
	return id;
}

static TypeId resolve_fn_spec (Sema *s, ASTNode *ref, uint8_t *done, int *deferred)
{
	uint16_t param_count = (uint16_t)ref->sym;
	uint16_t ret_count = (uint16_t)ref->u64;
	TypeId *params = arena_alloc(s->arena, sizeof(TypeId) * (param_count + 1u));
	TypeId *rets = arena_alloc(s->arena, sizeof(TypeId) * (ret_count + 1u));
	uint32_t item = ref->child;

	for (uint16_t i = 0; i < param_count; i++) {
		if (resolve_fn_item(s, &item, params + i, done, deferred, "parameter") == TYPE_ERROR)
			return TYPE_ERROR;
	}
	for (uint16_t i = 0; i < ret_count; i++) {
		if (resolve_fn_item(s, &item, rets + i, done, deferred, "return") == TYPE_ERROR)
			return TYPE_ERROR;
	}

	TypeId fn = type_fn_of(params, param_count, rets, ret_count,
			(ref->flags & NODE_FLAG_VARARG) != 0);

	if (fn == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
		return TYPE_ERROR;
	}
	return fn;
}

static TypeId resolve_spec_at (Sema *s, ASTNode *owner,
		uint8_t *done, int *deferred, int bare_fn)
{
	if (owner->type_node == NO_TYPE_NODE) return owner->data_type;

	uint32_t ref_idx = (uint32_t)(owner - s->ast->nodes) + owner->type_node;
	ASTNode *ref = s->ast->nodes + ref_idx;

	if (ref->type == NODE_TYPE_ARRAY || ref->type == NODE_TYPE_SLICE)
		return resolve_array_spec(s, ref, done, deferred,
				ref->type == NODE_TYPE_SLICE);
	if (ref->type == NODE_TYPE_FN) {
		TypeId fn = resolve_fn_spec(s, ref, done, deferred);

		if (fn == TYPE_ERROR || bare_fn) return fn;
		TypeId id = type_ptr_to(fn);

		if (id == TYPE_ERROR) {
			error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
			return TYPE_ERROR;
		}
		return id;
	}
	if (ref->type != NODE_TYPE_PTR)
		return resolve_type_ref(s, ref, done, deferred);

	TypeId id = resolve_spec_at(s, ref, done, deferred, 1);
	if (deferred && *deferred) return TYPE_ERROR;
	if (id == TYPE_ERROR) return TYPE_ERROR;
	if (!ptr_base_ok(id)) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"invalid pointer base type '%s'", types[id].name);
		return TYPE_ERROR;
	}

	id = type_ptr_to(id);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
		return TYPE_ERROR;
	}
	return id;
}

TypeId resolve_type_spec (Sema *s, ASTNode *owner, uint8_t *done, int *deferred)
{
	return resolve_spec_at(s, owner, done, deferred, 0);
}

TypeId resolve_type_node (Sema *s, ASTNode *n)
{
	if (n->type_node == NO_TYPE_NODE) return n->data_type;

	int deferred = 0;
	TypeId id = resolve_type_spec(s, n, NULL, &deferred);
	n->type_node = NO_TYPE_NODE;
	n->data_type = id;
	return id;
}

static void lower_ref_param (Sema *s, uint32_t idx)
{
	ASTNode *p = s->ast->nodes + idx;

	if (!(p->flags & NODE_FLAG_REF) || (p->flags & NODE_FLAG_REF_LOWERED))
		return;
	p->flags |= NODE_FLAG_REF_LOWERED;

	TypeId base = p->data_type;
	if (base == TYPE_ERROR) return;
	if (base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(p),
				"a reference cannot refer to void");
		p->data_type = TYPE_ERROR;
		return;
	}
	if (type_kind(base) == TK_ARRAY || type_kind(base) == TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(p),
				"references to arrays and slices are not supported");
		p->data_type = TYPE_ERROR;
		return;
	}
	TypeId ptr = type_ptr_to(base);
	if (ptr == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(p), "too many types");
		p->data_type = TYPE_ERROR;
		return;
	}
	p->data_type = ptr;
}

static void lower_variadic_param (Sema *s, uint32_t idx)
{
	ASTNode *p = s->ast->nodes + idx;
	TypeId elem = p->data_type;

	if (elem == TYPE_ERROR) return;
	if (p->flags & NODE_FLAG_REF_LOWERED) {
		error_report(s->err, ERR_ERROR, node_loc(p),
				"a variadic parameter cannot be a reference");
		p->data_type = TYPE_ERROR;
		return;
	}
	if (type_dyn_rank(elem) > 0) {
		error_report(s->err, ERR_ERROR, node_loc(p),
				"a variadic parameter cannot have a slice element type");
		p->data_type = TYPE_ERROR;
		return;
	}
	TypeId slice = type_slice_of(elem);
	if (slice == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(p),
				"the element type of a variadic parameter cannot be void");
		p->data_type = TYPE_ERROR;
		return;
	}
	p->data_type = slice;
}

void resolve_fn_types (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t param = s->ast->nodes[args].child;
	int typed = (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED) != 0;
	int lowered = (s->ast->nodes[args].flags & NODE_FLAG_VARARG_LOWERED) != 0;

	while (param != NO_NODE) {
		uint32_t next = s->ast->nodes[param].next_bro;

		resolve_type_node(s, s->ast->nodes + param);
		lower_ref_param(s, param);
		if (typed && !lowered && next == NO_NODE) lower_variadic_param(s, param);
		param = next;
	}
	if (typed) s->ast->nodes[args].flags |= NODE_FLAG_VARARG_LOWERED;
	resolve_type_node(s, s->ast->nodes + ret);
	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		resolve_type_node(s, s->ast->nodes + item);
}

uint32_t sema_ret_count (Sema *s, uint32_t ret)
{
	uint32_t count = 1;

	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		count++;
	return count;
}

uint32_t sema_ret_at (Sema *s, uint32_t ret, uint32_t i)
{
	uint32_t item = s->ast->nodes[ret].child;

	if (i == 0) return ret;
	for (uint32_t k = 1; k < i && item != NO_NODE; k++)
		item = s->ast->nodes[item].next_bro;
	return item;
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
	s->mod_name = NULL;
	s->mod_len = 0;
	s->mod_seen = 0;
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

static void reserve_cast_nodes (Sema *s)
{
	uint32_t old_cap = s->ast->cap;
	uint32_t new_cap = s->ast->count * 2 + 64;

	if (new_cap <= old_cap) return;

	s->ast->nodes = arena_realloc(s->arena, s->ast->nodes,
			(size_t)old_cap * sizeof(ASTNode), (size_t)new_cap * sizeof(ASTNode));
	s->ast->cap = new_cap;
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


static uint32_t check_array_scalar (Sema *s, TypeId elem, uint32_t item)
{
	TypeId type = check_expr(s, item);

	if (type == TYPE_ERROR || type == elem) return item;
	int ok;
	uint32_t cast = force_cast(s, item, type, elem, &ok);
	if (ok) return cast;

	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + item),
			"cannot initialize an array element of type %s"
			" with a value of type %s; use an explicit cast with 'as'",
			types[elem].name, type_display_name(type));
	return item;
}

static uint32_t check_array_elem (Sema *s, TypeId elem, uint32_t item)
{
	ASTNode *node = s->ast->nodes + item;
	TypeKind kind = type_kind(elem);
	int is_list = node->type == NODE_ARRAY_INIT;

	if (kind == TK_ARRAY && !is_list && node->type != NODE_LIT_STR)
		error_report(s->err, ERR_ERROR, node_loc(node),
				"array element of type %s requires a '{...}' initializer",
				types[elem].name);
	else if (kind == TK_ARRAY)
		check_array_init_at(s, elem, item);
	else if (kind == TK_STRUCT && (node->type == NODE_STRUCT_INIT || is_list))
		check_struct_init(s, item, elem);
	else if (is_list)
		error_report(s->err, ERR_ERROR, node_loc(node),
				"array element of type %s cannot be initialized"
				" with a '{...}' list", types[elem].name);
	else return check_array_scalar(s, elem, item);
	return item;
}

static uint32_t check_array_scalar (Sema *s, TypeId elem, uint32_t item);
static uint32_t check_array_elem (Sema *s, TypeId elem, uint32_t item);

static int array_init_designated (Sema *s, uint32_t init_idx)
{
	uint32_t item = s->ast->nodes[init_idx].child;

	return item != NO_NODE && s->ast->nodes[item].type == NODE_INDEX_INIT;
}

static uint32_t init_count (Sema *s, uint32_t init_idx)
{
	uint32_t count = 0;

	for (uint32_t item = s->ast->nodes[init_idx].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		count++;
	return count;
}

static void patch_array_dim (Sema *s, uint32_t ref_idx, uint32_t count)
{
	ASTNode lit = {0};

	lit.type = NODE_LIT_i64;
	lit.line = s->ast->nodes[ref_idx].line;
	lit.col = s->ast->nodes[ref_idx].col;
	lit.len = 1;
	lit.u64 = count;
	lit.child = NO_NODE;
	lit.next_bro = NO_NODE;
	lit.sym = NO_SYMBOL;
	lit.type_node = NO_TYPE_NODE;
	lit.data_type = TYPE_UNTYPED_INT;

	uint32_t lit_idx = sema_new_node(s, lit);

	s->ast->nodes[ref_idx].type = NODE_TYPE_ARRAY;
	s->ast->nodes[ref_idx].child = lit_idx;
}

static void infer_str_dim (Sema *s, uint32_t ref_idx, uint32_t init_idx)
{
	ASTNode *ref = s->ast->nodes + ref_idx;
	TypeId elem = ref->data_type;

	if (ref->type != NODE_TYPE_SLICE || ref->type_node != NO_TYPE_NODE)
		return;
	if (elem != TYPE_i8 && elem != TYPE_u8) return;
	patch_array_dim(s, ref_idx, (uint32_t)s->ast->nodes[init_idx].len + 1);
}

static uint32_t type_next (Sema *s, uint32_t ref_idx)
{
	ASTNode *ref = s->ast->nodes + ref_idx;

	if (ref->type_node == NO_TYPE_NODE) return NO_NODE;
	return ref_idx + ref->type_node;
}

static int designated_item_index (Sema *s, uint32_t item_idx, uint32_t depth, uint64_t *out)
{
	ASTNode *it = s->ast->nodes + item_idx;
	uint32_t node = it->child;

	if (it->sym <= depth) return 0;
	for (uint32_t k = 0; k < depth; k++)
		node = s->ast->nodes[node].next_bro;
	return dim_value(s, node, out);
}

static uint32_t designated_level_count (Sema *s, uint32_t init_idx, uint32_t depth)
{
	uint32_t item = s->ast->nodes[init_idx].child;
	uint64_t max = 0;
	int any = 0;

	while (item != NO_NODE) {
		uint64_t v;

		if (designated_item_index(s, item, depth, &v)) {
			if (!any || v > max) max = v;
			any = 1;
		}
		item = s->ast->nodes[item].next_bro;
	}
	if (!any) return 0;
	if (max >= UINT32_MAX) return UINT32_MAX;
	return (uint32_t)(max + 1);
}

static void infer_designated_levels (Sema *s, uint32_t ref_idx, uint32_t init_idx)
{
	uint32_t ref = ref_idx;
	uint32_t depth = 0;

	while (1) {
		ASTNode *node = s->ast->nodes + ref;
		uint32_t count;

		if (node->type != NODE_TYPE_ARRAY && node->type != NODE_TYPE_SLICE) return;
		count = designated_level_count(s, init_idx, depth);
		if (count && node->type == NODE_TYPE_SLICE)
			patch_array_dim(s, ref, count);
		node = s->ast->nodes + ref;
		if (node->type_node == NO_TYPE_NODE) return;
		ref += node->type_node;
		depth++;
	}
}

static void infer_init_level (Sema *s, uint32_t ref_idx, uint32_t init_idx);

static void infer_designated_values (Sema *s, uint32_t ref_idx, uint32_t init_idx)
{
	uint32_t item = s->ast->nodes[init_idx].child;
	uint32_t elem = type_next(s, ref_idx);

	if (elem == NO_NODE) return;
	while (item != NO_NODE) {
		ASTNode *it = s->ast->nodes + item;
		uint32_t ref = elem;

		for (uint32_t k = 1; k < it->sym && ref != NO_NODE; k++)
			ref = type_next(s, ref);
		if (ref != NO_NODE) infer_init_level(s, ref, (uint32_t)it->u64);
		item = it->next_bro;
	}
}

static void infer_designated (Sema *s, uint32_t ref_idx, uint32_t init_idx)
{
	infer_designated_levels(s, ref_idx, init_idx);
	infer_designated_values(s, ref_idx, init_idx);
}

static void infer_init_level (Sema *s, uint32_t ref_idx, uint32_t init_idx)
{
	ASTNode *ref = s->ast->nodes + ref_idx;
	ASTNode *init = s->ast->nodes + init_idx;

	if (ref->type != NODE_TYPE_ARRAY && ref->type != NODE_TYPE_SLICE) return;
	if (init->type == NODE_LIT_STR) {
		infer_str_dim(s, ref_idx, init_idx);
		return;
	}
	if (init->type != NODE_ARRAY_INIT) return;
	if (array_init_designated(s, init_idx)) {
		infer_designated(s, ref_idx, init_idx);
		return;
	}

	if (ref->type == NODE_TYPE_SLICE)
		patch_array_dim(s, ref_idx, init_count(s, init_idx));

	ref = s->ast->nodes + ref_idx;
	if (ref->type_node == NO_TYPE_NODE) return;
	uint32_t next = ref_idx + ref->type_node;
	for (uint32_t item = s->ast->nodes[init_idx].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		infer_init_level(s, next, item);
}

void infer_array_dims (Sema *s, uint32_t decl_idx)
{
	ASTNode *decl = s->ast->nodes + decl_idx;

	if (decl->type != NODE_ARR_DEC || decl->type_node == NO_TYPE_NODE
			|| decl->child == NO_NODE)
		return;
	infer_init_level(s, decl_idx + decl->type_node, decl->child);
}

static void fold_index_lit (Sema *s, uint32_t idx, uint64_t value)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t next = n->next_bro;

	n->type = NODE_LIT_i64;
	n->i64 = (int64_t)value;
	n->data_type = TYPE_UNTYPED_INT;
	n->child = NO_NODE;
	n->type_node = NO_TYPE_NODE;
	n->sym = NO_SYMBOL;
	n->next_bro = next;
}

static int designated_target (Sema *s, TypeId arr, uint32_t item_idx, TypeId *target)
{
	ASTNode *item = s->ast->nodes + item_idx;
	uint32_t node = item->child;
	TypeId level = arr;
	uint32_t k;

	for (k = 0; k < item->sym; k++) {
		ASTNode *dim = s->ast->nodes + node;
		uint64_t v;

		if (type_kind(level) != TK_ARRAY) break;
		if (!dim_value(s, node, &v)) {
			error_report(s->err, ERR_ERROR, node_loc(dim),
					"array initializer index must be a constant integer expression");
			return 0;
		}
		if (v >= types[level].count) {
			error_report(s->err, ERR_ERROR, node_loc(dim),
					"array initializer index %lld is out of bounds for an array of size %u",
					(long long)(int64_t)v, types[level].count);
			return 0;
		}
		uint32_t next = dim->next_bro;

		fold_index_lit(s, node, v);
		node = next;
		level = types[level].base;
	}
	if (k != item->sym) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + item_idx),
				"array initializer has too many indices");
		return 0;
	}
	*target = level;
	return 1;
}

static int designated_overlap (Sema *s, uint32_t a, uint32_t b)
{
	ASTNode *ia = s->ast->nodes + a;
	ASTNode *ib = s->ast->nodes + b;
	uint32_t x = ia->child;
	uint32_t y = ib->child;
	uint32_t n = ia->sym < ib->sym ? ia->sym : ib->sym;

	for (uint32_t k = 0; k < n; k++) {
		uint64_t vx;
		uint64_t vy;

		if (!dim_value(s, x, &vx) || !dim_value(s, y, &vy)) return 0;
		if (vx != vy) return 0;
		x = s->ast->nodes[x].next_bro;
		y = s->ast->nodes[y].next_bro;
	}
	return 1;
}

static void check_designated_init (Sema *s, TypeId arr, uint32_t init_idx)
{
	uint32_t item = s->ast->nodes[init_idx].child;

	while (item != NO_NODE) {
		uint32_t next = s->ast->nodes[item].next_bro;
		TypeId target;
		int dup = 0;

		for (uint32_t prev = s->ast->nodes[init_idx].child; prev != item;
				prev = s->ast->nodes[prev].next_bro) {
			if (designated_overlap(s, prev, item)) {
				error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + item),
						"duplicate array initializer index");
				dup = 1;
				break;
			}
		}
		if (!dup && designated_target(s, arr, item, &target)) {
			uint32_t old = (uint32_t)s->ast->nodes[item].u64;
			uint32_t out = check_array_elem(s, target, old);

			if (out != old) s->ast->nodes[item].u64 = out;
		}
		item = next;
	}
}

void check_array_init_at (Sema *s, TypeId arr, uint32_t init_idx)
{
	TypeId elem = types[arr].base;
	uint32_t count = types[arr].count;
	ASTNode *init = s->ast->nodes + init_idx;

	if (init->type == NODE_LIT_STR) {
		if (types[elem].size != 1 || (elem != TYPE_i8 && elem != TYPE_u8))
			error_report(s->err, ERR_ERROR, node_loc(init),
					"string initializer requires an i8, u8 or char array");
		else if ((uint32_t)init->len + 1 > count)
			error_report(s->err, ERR_ERROR, node_loc(init),
					"string literal needs %u bytes including the terminator, but the array has room for %u",
					(unsigned)init->len + 1, count);
		init->data_type = TYPE_STR;
		return;
	}
	if (init->type != NODE_ARRAY_INIT) {
		error_report(s->err, ERR_ERROR, node_loc(init),
				"array initializer must be a '{...}' list or a string literal");
		return;
	}
	if (type_dyn_rank(arr) > 0) {
		error_report(s->err, ERR_ERROR, node_loc(init),
				"an array type with '[]' dimensions cannot be initialized with '{...}'");
		return;
	}
	if (array_init_designated(s, init_idx)) {
		check_designated_init(s, arr, init_idx);
		return;
	}
	ErrorLoc init_loc = node_loc(init);

	uint32_t n = 0;
	uint32_t item = s->ast->nodes[init_idx].child;
	uint32_t prev = NO_NODE;

	while (item != NO_NODE) {
		uint32_t next = s->ast->nodes[item].next_bro;
		uint32_t out = check_array_elem(s, elem, item);

		if (out != item && prev == NO_NODE) s->ast->nodes[init_idx].child = out;
		else if (out != item) s->ast->nodes[prev].next_bro = out;
		prev = out;
		item = next;
		n++;
	}

	if (n != count)
		error_report(s->err, ERR_ERROR, init_loc,
				"array initializer must have exactly %u elements (found %u)", count, n);
}

void check_array_init (Sema *s, uint32_t decl_idx)
{
	ASTNode *d = s->ast->nodes + decl_idx;

	check_array_init_at(s, d->data_type, d->child);
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
			(int)n->len, n->str, types[n->data_type].name,
			type_display_name(init_type), hint);
}

void record_init_order (Sema *s, uint32_t idx)
{
	if (s->init_order_count >= s->init_order_cap) {
		uint32_t old_cap = s->init_order_cap;
		s->init_order_cap <<= 1;
		s->init_order = arena_realloc(s->arena, s->init_order,
				old_cap * sizeof(uint32_t), s->init_order_cap * sizeof(uint32_t));
	}
	s->init_order[s->init_order_count++] = idx;
}

void record_static (Sema *s, uint32_t idx)
{
	if (s->static_count >= s->static_cap) {
		uint32_t old_cap = s->static_cap;
		s->static_cap <<= 1;
		s->statics = arena_realloc(s->arena, s->statics,
				old_cap * sizeof(uint32_t), s->static_cap * sizeof(uint32_t));
	}
	s->statics[s->static_count++] = idx;
}

static int is_const_ptr_init (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	while (n->type == NODE_CAST && n->child != NO_NODE)
		n = &s->ast->nodes[n->child];

	if (n->type == NODE_LIT_NULL || n->type == NODE_LIT_STR) return 1;
	if (n->type == NODE_ID && n->sym != NO_SYMBOL)
		return s->table.symbols[n->sym].kind == SYMBOL_FN;
	if (n->type != NODE_ADDR || n->child == NO_NODE) return 0;

	ASTNode *child = &s->ast->nodes[n->child];

	if (child->type != NODE_ID || child->sym == NO_SYMBOL) return 0;
	return s->table.symbols[child->sym].depth == 0;
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

	if (n->data_type != TYPE_ERROR && type_kind(n->data_type) == TK_ARRAY) {
		check_array_init(s, idx);
	} else if (n->data_type != TYPE_ERROR && type_kind(n->data_type) == TK_STRUCT
			&& (s->ast->nodes[n->child].type == NODE_STRUCT_INIT
				|| s->ast->nodes[n->child].type == NODE_ARRAY_INIT)) {
		check_struct_init(s, n->child, n->data_type);
	} else {
		TypeId init_type = check_expr(s, n->child);
		n = s->ast->nodes + idx;

		if (n->data_type != TYPE_ERROR && type_kind(n->data_type) == TK_PTR && init_type != TYPE_ERROR
				&& (type_kind(init_type) == TK_PTR || init_type == TYPE_NULL
				|| init_type == TYPE_STR) && !is_const_ptr_init(s, n->child))
			error_report(s->err, ERR_ERROR, node_loc(n),
					"global pointer initializers only support NULL, a string literal or the address of a global");

		check_var_init_type(s, n, init_type);
	}
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

	TypeId id = add_type(n->str, n->len, TK_ENUM, TYPE_u32);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return;
	}
	sema_declare(s, n->str, n->len, SYMBOL_ENUM, id, idx);
}

static void decl_types (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_ENUM_DEC) decl_enum(s, idx);
		else if (n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC)
			sema_decl_type(s, idx);
		else if (n->type == NODE_STRUCT_DEC) sema_decl_struct(s, idx);
		idx = n->next_bro;
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
	if (type_kind(base) != TK_INT) {
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
		if (s->ast->nodes[idx].type == NODE_ENUM_DEC)
			resolve_enum_base(s, idx);
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

	if (n->sym == NO_SYMBOL) return;
	Symbol *sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR) return;

	uint16_t count = 0;
	for (uint32_t var = n->child; var != NO_NODE; var = s->ast->nodes[var].next_bro)
		count++;

	if (count == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enum '%.*s' must have at least one variant",
				(int)n->len, n->str);
		return;
	}

	EnumVariant *vars = arena_alloc(s->arena, sizeof(EnumVariant) * count);
	count = 0;
	uint64_t prev = 0;

	for (uint32_t var = n->child; var != NO_NODE; var = s->ast->nodes[var].next_bro) {
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
		vars[count].decl = var;
		vars[count].used = 0;
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

static void check_module_decl (Sema *s, ASTNode *n)
{
	if (s->mod_seen) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"duplicate 'module' declaration");
		return;
	}
	s->mod_seen = 1;
	if (!n->len) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"module name cannot be empty");
		return;
	}
	s->mod_name = n->str;
	s->mod_len = n->len;
}

static void check_root (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC) {
			if (n->flags & NODE_FLAG_STATIC)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"static variables can only be declared inside a function");
			check_global_var_init(s, idx);
		}
		else if (n->type == NODE_FN_DEC) check_fn_dec(s, idx);
		else if (n->type == NODE_EXTERN_FN_DEC) check_extern_fn(s, idx);
		else if (n->type == NODE_MODULE_DEC) check_module_decl(s, n);
		else if (n->type != NODE_ENUM_DEC && n->type != NODE_TYPE_DEC
				&& n->type != NODE_NEWTYPE_DEC && n->type != NODE_EXTERN_VAR_DEC
				&& n->type != NODE_STRUCT_DEC && n->type != NODE_IMPORT_DEC
				&& n->type != NODE_FROM_DEC)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"only variable, function, enum and type declarations are allowed at the top level");
		idx = n->next_bro;
	}
}

static void reject_untyped_global (Sema *s, ASTNode *n)
{
	if (n->type != NODE_VAR_DEC && n->type != NODE_ARR_DEC) return;
	if (n->type_node != NO_TYPE_NODE || n->data_type != TYPE_VOID) return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot infer the type of global '%.*s'; add an explicit type",
			(int)n->len, n->str);
	n->data_type = TYPE_ERROR;
}

static TypeId build_fn_value (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t param = s->ast->nodes[args].child;
	uint16_t param_count = 0;
	uint16_t ret_count = (uint16_t)sema_ret_count(s, ret);
	uint16_t used = 0;
	uint16_t k = 0;

	if (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED) return TYPE_ERROR;
	for (uint32_t p = param; p != NO_NODE; p = s->ast->nodes[p].next_bro) {
		if (s->ast->nodes[p].child != NO_NODE) return TYPE_ERROR;
		param_count++;
	}

	TypeId *params = arena_alloc(s->arena, sizeof(TypeId) * (param_count + 1u));
	TypeId *rets = arena_alloc(s->arena, sizeof(TypeId) * (ret_count + 1u));

	for (uint32_t p = param; p != NO_NODE; p = s->ast->nodes[p].next_bro) {
		TypeId t = s->ast->nodes[p].data_type;

		if (t == TYPE_ERROR) return TYPE_ERROR;
		params[k++] = t;
	}
	for (uint16_t i = 0; i < ret_count; i++) {
		TypeId t = s->ast->nodes[sema_ret_at(s, ret, i)].data_type;

		if (t == TYPE_ERROR) return TYPE_ERROR;
		if (t == TYPE_VOID && ret_count == 1) continue;
		rets[used++] = t;
	}

	TypeId fn = type_fn_of(params, param_count, rets, used,
			(s->ast->nodes[args].flags & NODE_FLAG_VARARG) != 0);

	if (fn == TYPE_ERROR) return TYPE_ERROR;
	return type_ptr_to(fn);
}

static void decl_globals (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_FN_DEC || n->type == NODE_EXTERN_FN_DEC) {
			uint32_t ret = s->ast->nodes[n->child].next_bro;
			resolve_fn_types(s, idx);
			uint32_t sym = sema_declare(s, n->str, n->len, SYMBOL_FN,
					s->ast->nodes[ret].data_type, idx);
			s->table.symbols[sym].fn_type = build_fn_value(s, idx);
		} else if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC
				|| n->type == NODE_EXTERN_VAR_DEC) {
			if (n->child != NO_NODE) infer_array_dims(s, idx);
			resolve_type_node(s, n);
			check_storage_decl(s, n);
			reject_untyped_global(s, n);
			if (n->len == 4 && memcmp(n->str, "main", 4) == 0)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"'main' must be a function");
			sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);
		}

		idx = n->next_bro;
	}
}

static int is_extern_decl (Sema *s, Symbol *sym)
{
	ASTNode *n = s->ast->nodes + sym->decl;

	return sym->decl != NO_NODE
			&& (n->type == NODE_EXTERN_FN_DEC || n->type == NODE_EXTERN_VAR_DEC);
}

static void mark_exposed_type (TypeId type);

static void mark_exposed_struct (TypeId id)
{
	Type *t = types + id;

	if (t->exposed || t->layout != 2) return;
	t->exposed = 1;
	for (uint16_t i = 0; i < t->field_count; i++)
		mark_exposed_type(t->fields[i].type);
}

static void mark_exposed_type (TypeId type)
{
	for (int i = 0; i < TYPE_MAX; i++) {
		TypeKind kind = type_kind(type);

		if (kind != TK_PTR && kind != TK_ARRAY && kind != TK_SLICE
				&& kind != TK_ALIAS)
			break;
		type = types[type].base;
	}
	if (type_kind(type) == TK_STRUCT) mark_exposed_struct(type);
}

static void mark_exposed_sig (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t p = s->ast->nodes[args].child;

	while (p != NO_NODE) {
		mark_exposed_type(s->ast->nodes[p].data_type);
		p = s->ast->nodes[p].next_bro;
	}
	mark_exposed_type(s->ast->nodes[ret].data_type);
	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		mark_exposed_type(s->ast->nodes[item].data_type);
}

static void mark_exposed_decls (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		int exported = (n->flags & NODE_FLAG_EXPORT) != 0;

		if (n->type == NODE_EXTERN_FN_DEC || (n->type == NODE_FN_DEC && exported))
			mark_exposed_sig(s, idx);
		else if (n->type == NODE_EXTERN_VAR_DEC
				|| ((n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC) && exported))
			mark_exposed_type(n->data_type);
		else if (exported && (n->type == NODE_STRUCT_DEC
				|| n->type == NODE_ENUM_DEC || n->type == NODE_TYPE_DEC
				|| n->type == NODE_NEWTYPE_DEC))
			mark_exposed_type(n->data_type);
		idx = n->next_bro;
	}
}

static ASTNode *named_type_decl (Sema *s, TypeId id)
{
	if (type_kind(id) == TK_STRUCT) {
		uint32_t decl = types[id].decl;

		if (decl < s->ast->count) return s->ast->nodes + decl;
	}
	for (uint32_t idx = s->ast->nodes[0].child; idx != NO_NODE;
			idx = s->ast->nodes[idx].next_bro) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type != NODE_ENUM_DEC && n->type != NODE_TYPE_DEC
				&& n->type != NODE_NEWTYPE_DEC)
			continue;
		if (n->sym != NO_SYMBOL && s->table.symbols[n->sym].type == id)
			return n;
	}
	return NULL;
}

static void require_public_type (Sema *s, TypeId type, uint32_t use);

static void require_public_fields (Sema *s, uint32_t first)
{
	for (uint32_t m = first; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *n = s->ast->nodes + m;

		if (n->type == NODE_STRUCT_DEC)
			require_public_fields(s, n->child);
		else if (n->type == NODE_FIELD_DEC
				&& !(n->flags & NODE_FLAG_BITFIELD))
			require_public_type(s, n->data_type, m);
	}
}

static void require_public_type (Sema *s, TypeId type, uint32_t use)
{
	if (type == TYPE_ERROR || type < TYPE_COUNT) return;

	switch (type_kind(type))
	{
	case TK_PTR: case TK_ARRAY: case TK_SLICE:
		require_public_type(s, types[type].base, use);
		return;
	case TK_FN:
		for (uint16_t i = 0; i < types[type].param_count; i++)
			require_public_type(s, types[type].params[i], use);
		for (uint16_t i = 0; i < types[type].ret_count; i++)
			require_public_type(s, types[type].rets[i], use);
		return;
	case TK_STRUCT: case TK_ENUM: case TK_ALIAS: case TK_NEWTYPE: {
		ASTNode *decl = named_type_decl(s, type);

		if (decl && !(decl->flags & NODE_FLAG_PUB))
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
					"public declaration exposes non-public type '%.*s'",
					(int)types[type].len, types[type].name);
		return;
	}
	default:
		return;
	}
}

static void require_public_sig (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;

	for (uint32_t p = s->ast->nodes[args].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro)
		require_public_type(s, s->ast->nodes[p].data_type, p);
	require_public_type(s, s->ast->nodes[ret].data_type, ret);
	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		require_public_type(s, s->ast->nodes[item].data_type, item);
}

static void require_public_decl (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_FN_DEC: case NODE_EXTERN_FN_DEC:
		require_public_sig(s, idx);
		return;
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_EXTERN_VAR_DEC:
		require_public_type(s, n->data_type, idx);
		return;
	case NODE_STRUCT_DEC:
		require_public_fields(s, n->child);
		return;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		require_public_type(s, n->data_type, idx);
		return;
	default:
		return;
	}
}

static void check_public_types (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		uint16_t flags = s->ast->nodes[idx].flags;

		if ((flags & NODE_FLAG_PUB) && !(flags & NODE_FLAG_EXPORT))
			require_public_decl(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static int c_keyword (const char *name, uint16_t len)
{
	static const char *words[] = {
		"auto", "break", "case", "char", "const", "continue", "default", "do",
		"double", "else", "enum", "extern", "float", "for", "goto", "if",
		"inline", "int", "long", "register", "restrict", "return", "short",
		"signed", "sizeof", "static", "struct", "switch", "typedef", "union",
		"unsigned", "void", "volatile", "while", "_Bool", "_Complex",
		"_Imaginary", "_Alignas", "_Alignof", "_Atomic", "_Generic",
		"_Noreturn", "_Static_assert", "_Thread_local", "alignas", "alignof",
		"and", "and_eq", "asm", "bitand", "bitor", "bool", "catch", "class",
		"compl", "concept", "consteval", "constexpr", "constinit", "const_cast",
		"co_await", "co_return", "co_yield", "decltype", "delete",
		"dynamic_cast", "explicit", "export", "false", "friend", "mutable",
		"namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator",
		"or", "or_eq", "private", "protected", "public", "reinterpret_cast",
		"requires", "static_assert", "static_cast", "template", "this",
		"thread_local", "throw", "true", "try", "typeid", "typename", "using",
		"virtual", "wchar_t", "xor", "xor_eq"
	};
	for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
		if (strlen(words[i]) == len && memcmp(words[i], name, len) == 0)
			return 1;
	return 0;
}

static inline void check_export_name (Sema *s, ASTNode *n)
{
	if (n->str && c_keyword(n->str, n->len))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"exported name '%.*s' is a C/C++ keyword; rename it",
				(int)n->len, n->str);
}

static void check_export_type (Sema *s, TypeId type, uint32_t use)
{
	if (type == TYPE_ERROR || type < TYPE_COUNT) return;
	switch (type_kind(type))
	{
	case TK_PTR: case TK_ARRAY: case TK_SLICE:
		check_export_type(s, types[type].base, use);
		return;
	case TK_FN: {
		Type *fn = types + type;

		for (uint16_t i = 0; i < fn->param_count; i++)
			check_export_type(s, fn->params[i], use);
		for (uint16_t i = 0; i < fn->ret_count; i++)
			check_export_type(s, fn->rets[i], use);
		return;
	}
	case TK_STRUCT: case TK_ENUM: case TK_ALIAS: case TK_NEWTYPE: {
		ASTNode *decl = named_type_decl(s, type);

		if (decl && !(decl->flags & NODE_FLAG_EXPORT))
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
					"export declaration exposes non-export type '%.*s'",
					(int)types[type].len, types[type].name);
		return;
	}
	default: return;
	}
}

static void check_export_fields (Sema *s, uint32_t first)
{
	for (uint32_t m = first; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *n = s->ast->nodes + m;

		if (n->type == NODE_STRUCT_DEC) check_export_fields(s, n->child);
		else if (n->type == NODE_FIELD_DEC)
			check_export_type(s, n->data_type, m);
	}
}

static void check_export_decl (Sema *s, ASTNode *n, uint32_t idx)
{
	switch (n->type)
	{
	case NODE_FN_DEC: case NODE_EXTERN_FN_DEC: {
		uint32_t args = n->child;
		uint32_t ret = s->ast->nodes[args].next_bro;
		uint32_t item;

		for (item = s->ast->nodes[args].child; item != NO_NODE;
				item = s->ast->nodes[item].next_bro)
			check_export_type(s, s->ast->nodes[item].data_type, item);
		check_export_type(s, s->ast->nodes[ret].data_type, ret);
		for (item = s->ast->nodes[ret].child; item != NO_NODE;
				item = s->ast->nodes[item].next_bro)
			check_export_type(s, s->ast->nodes[item].data_type, item);
		return;
	}
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_EXTERN_VAR_DEC:
		check_export_type(s, n->data_type, idx);
		return;
	case NODE_STRUCT_DEC: check_export_fields(s, n->child); return;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		check_export_type(s, n->data_type, idx);
		return;
	default: return;
	}
}

void check_export_decls (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->flags & NODE_FLAG_EXPORT) {
			check_export_name(s, n);
			check_export_decl(s, n, idx);
		}
		idx = n->next_bro;
	}
}

static const char *type_decl_kind (ASTNode *n)
{
	switch (n->type)
	{
	case NODE_STRUCT_DEC: return (n->flags & NODE_FLAG_UNION) ? "union" : "struct";
	case NODE_ENUM_DEC: return "enum";
	case NODE_NEWTYPE_DEC: return "newtype";
	case NODE_TYPE_DEC: return "type";
	default: return NULL;
	}
}

static void warn_unused_fields (Sema *s, Type *t)
{
	if (t->layout != 2) return;

	for (uint16_t i = 0; i < t->field_count; i++) {
		StructField *f = t->fields + i;

		if (f->used || f->name[0] == '_') continue;
		error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + f->decl),
				"unused field '%.*s' of %s '%s'", (int)f->len, f->name,
				t->is_union ? "union" : "struct", t->name);
	}
}

static void warn_unused_variants (Sema *s, Type *t)
{
	for (uint16_t i = 0; i < t->variant_count; i++) {
		EnumVariant *v = t->variants + i;

		if (v->used || v->name[0] == '_') continue;
		error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + v->decl),
				"unused variant '%.*s' of enum '%s'", (int)v->len, v->name, t->name);
	}
}

static void warn_unused_type (Sema *s, Symbol *sym)
{
	if (sym->decl == NO_NODE) return;
	ASTNode *decl = s->ast->nodes + sym->decl;
	const char *kind = type_decl_kind(decl);
	Type *t = types + sym->type;
	if (!kind || (decl->flags & NODE_FLAG_PUB) || t->exposed) return;
	if (!t->used) {
		if (sym->name[0] == '_') return;
		error_report(s->err, ERR_WARNING, node_loc(decl), "unused %s '%.*s'",
				kind, (int)sym->len, sym->name);
		return;
	}
	if (decl->type == NODE_STRUCT_DEC) warn_unused_fields(s, t);
	else if (decl->type == NODE_ENUM_DEC) warn_unused_variants(s, t);
}

static void warn_unused_top (Sema *s)
{
	for (uint32_t i = 0; i < s->table.count; i++) {
		Symbol *sym = s->table.symbols + i;
		ErrorLoc loc = { .line = sym->line, .col = sym->col, .len = sym->len };

		if (sym->depth != 0) continue;
		if (sym->decl != NO_NODE && (s->ast->nodes[sym->decl].flags
				& (NODE_FLAG_PUB | NODE_FLAG_EXPORT)))
			continue;
		if (sym->kind == SYMBOL_TYPE || sym->kind == SYMBOL_ENUM) {
			if (sym->type != TYPE_ERROR) warn_unused_type(s, sym);
			continue;
		}
		if (sym->read || is_extern_decl(s, sym) || sym->name[0] == '_') continue;

		if (sym->kind == SYMBOL_FN && !(sym->len == 4
				&& memcmp(sym->name, "main", 4) == 0))
			error_report(s->err, ERR_WARNING, loc,
					"unused function '%.*s'", (int)sym->len, sym->name);
		else if (sym->kind == SYMBOL_VAR)
			error_report(s->err, ERR_WARNING, loc,
					"unused variable '%.*s'", (int)sym->len, sym->name);
	}
}

void sema_run (Sema *s)
{
	reserve_cast_nodes(s);

	decl_types(s);
	sema_resolve_types(s);
	resolve_enum_bases(s);
	sema_finalize_types(s);
	sema_layout_structs(s);
	decl_globals(s);
	bind_enums(s);
	imports_check_collisions(s);

	check_fn_defaults_all(s);
	check_root(s);
	mark_exposed_decls(s);
	check_public_types(s);
	check_export_decls(s);
	warn_unused_top(s);
}

// debug shit
static const char *kind_name (uint8_t kind)
{
	switch (kind)
	{
	case SYMBOL_FN: return "fn";
	case SYMBOL_VAR: return "var";
	case SYMBOL_PARAM: return "param";
	case SYMBOL_ENUM: return "enum";
	case SYMBOL_TYPE: return "type";
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

static void dump_fields (TypeId id)
{
	const Type *t = types + id;

	for (uint16_t i = 0; i < t->field_count; i++) {
		const StructField *f = t->fields + i;

		printf("  field %.*s type=%s offset=%u", (int)f->len, f->name,
				types[f->type].name, f->offset);
		if (f->bits) printf(" bits=%u bit_off=%u", f->bits, f->bit_off);
		printf("\n");
	}
}

static void dump_ret_items (Sema *s, Symbol *sym)
{
	if (sym->decl == NO_NODE) return;

	uint32_t ret = s->ast->nodes[s->ast->nodes[sym->decl].child].next_bro;
	uint32_t item = s->ast->nodes[ret].child;

	while (item != NO_NODE) {
		printf(", %s", types[s->ast->nodes[item].data_type].name);
		item = s->ast->nodes[item].next_bro;
	}
}

void dump_symbols (Sema *s)
{
	SymbolTable *t = &s->table;

	for (uint32_t i = 0; i < t->count; i++) {
		Symbol *sym = t->symbols + i;

		if (sym->kind == SYMBOL_ENUM && type_kind(sym->type) == TK_ENUM) {
			TypeId base = types[sym->type].base;

			printf("enum %.*s [%u:%u] depth=%u type=%s base=%s\n",
					(int)sym->len, sym->name, sym->line, sym->col,
					sym->depth, types[sym->type].name, types[base].name);
			dump_variants(sym->type);
			continue;
		}
		if (sym->kind == SYMBOL_TYPE && type_kind(sym->type) == TK_STRUCT) {
			printf("%s %.*s [%u:%u] depth=%u size=%u align=%u%s\n",
					types[sym->type].is_union ? "union" : "struct",
					(int)sym->len, sym->name, sym->line, sym->col, sym->depth,
					types[sym->type].size, types[sym->type].align,
					types[sym->type].is_packed ? " packed" : "");
			dump_fields(sym->type);
			continue;
		}
		if (sym->kind == SYMBOL_TYPE) {
			TypeId id = sym->type;
			const char *kind = type_kind(id) == TK_NEWTYPE
					? "newtype" : "type";

			if (id == TYPE_ERROR || types[id].base == TYPE_ERROR)
				printf("%s %.*s [%u:%u] depth=%u type=error\n",
						kind, (int)sym->len, sym->name,
						sym->line, sym->col, sym->depth);
			else printf("%s %.*s [%u:%u] depth=%u type=%s base=%s\n",
					kind, (int)sym->len, sym->name,
					sym->line, sym->col, sym->depth,
					types[id].name, types[types[id].base].name);
			continue;
		}
		printf("%s%s %.*s [%u:%u] depth=%u type=%s",
				sym->is_static ? "static " : "", kind_name(sym->kind),
				(int)sym->len, sym->name,
				sym->line, sym->col, sym->depth, types[sym->type].name);
		if (sym->kind == SYMBOL_FN) dump_ret_items(s, sym);
		printf("\n");
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
