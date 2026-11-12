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

static int ptr_base_ok (TypeId base)
{
	TypeKind kind = type_kind(base);

	return kind == TK_VOID || kind == TK_INT || kind == TK_ENUM
			|| kind == TK_NEWTYPE || kind == TK_PTR || kind == TK_STRUCT;
}

static TypeId resolve_type_ref (Sema *s, ASTNode *ref, uint8_t *done, int *deferred)
{
	TypeId id = find_type(ref->str, ref->len);

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

static int dim_value (Sema *s, uint32_t idx, uint64_t *out)
{
	ASTNode *n = s->ast->nodes + idx;
	uint64_t a;

	switch (n->type)
	{
	case NODE_LIT_i64:
	case NODE_LIT_u64: *out = n->u64; return 1;
	case NODE_LIT_CHAR: *out = (uint64_t)(int64_t)n->chr; return 1;
	case NODE_CAST:
		if (!dim_value(s, n->child, &a)) return 0;
		*out = trunc_dim(a, n->data_type);
		return 1;
	case NODE_NEG:
		if (!dim_value(s, n->child, &a)) return 0;
		*out = 0 - a;
		return 1;
	case NODE_NOT_A:
		if (!dim_value(s, n->child, &a)) return 0;
		*out = ~a;
		return 1;
	default:
		break;
	}

	if (n->child == NO_NODE) return 0;
	if (!dim_value(s, n->child, &a)) return 0;

	uint64_t b;
	if (!dim_value(s, s->ast->nodes[n->child].next_bro, &b)) return 0;

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

TypeId resolve_type_spec (Sema *s, ASTNode *owner, uint8_t *done, int *deferred)
{
	if (owner->type_node == NO_TYPE_NODE) return owner->data_type;

	uint32_t ref_idx = (uint32_t)(owner - s->ast->nodes) + owner->type_node;
	ASTNode *ref = s->ast->nodes + ref_idx;

	if (ref->type == NODE_TYPE_ARRAY || ref->type == NODE_TYPE_SLICE)
		return resolve_array_spec(s, ref, done, deferred,
				ref->type == NODE_TYPE_SLICE);
	if (ref->type != NODE_TYPE_PTR)
		return resolve_type_ref(s, ref, done, deferred);

	TypeId id = resolve_type_spec(s, ref, done, deferred);
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

TypeId resolve_type_node (Sema *s, ASTNode *n)
{
	if (n->type_node == NO_TYPE_NODE) return n->data_type;

	int deferred = 0;
	TypeId id = resolve_type_spec(s, n, NULL, &deferred);
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

	ASTNode *decl = s->ast->nodes + decl_node;
	int aggregate = decl->data_type != TYPE_ERROR
			&& type_is_aggregate(decl->data_type);

	if (kind == SYMBOL_PARAM || (kind == SYMBOL_VAR && (decl->child != NO_NODE 
			|| decl->type == NODE_EXTERN_VAR_DEC || aggregate)))
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

	if (is_arr && kind != TK_ARRAY) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'arr' declarations must have an array type");
		n->data_type = TYPE_ERROR;
		return;
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
	ErrorLoc init_loc = node_loc(init);

	uint32_t n = 0;
	uint32_t item = s->ast->nodes[init_idx].child;
	uint32_t prev = NO_NODE;

	while (item != NO_NODE) {
		uint32_t next = s->ast->nodes[item].next_bro;
		ASTNode *item_node = s->ast->nodes + item;

		if (type_kind(elem) == TK_ARRAY) {
			if (item_node->type != NODE_ARRAY_INIT
					&& item_node->type != NODE_LIT_STR)
				error_report(s->err, ERR_ERROR, node_loc(item_node),
						"array element of type %s requires a '{...}' initializer",
						types[elem].name);
			else
				check_array_init_at(s, elem, item);
		} else if (type_kind(elem) == TK_STRUCT && (item_node->type == NODE_STRUCT_INIT
				|| item_node->type == NODE_ARRAY_INIT)) {
			check_struct_init(s, item, elem);
		} else if (item_node->type == NODE_ARRAY_INIT) {
			error_report(s->err, ERR_ERROR, node_loc(item_node),
					"array element of type %s cannot be initialized"
					" with a '{...}' list", types[elem].name);
		} else {
			TypeId type = check_expr(s, item);

			if (type != TYPE_ERROR && type != elem) {
				int ok;
				uint32_t cast = force_cast(s, item, type, elem, &ok);

				if (!ok) {
					error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + item),
							"cannot initialize an array element of type %s"
							" with a value of type %s; use an explicit cast with 'as'",
							types[elem].name, type_display_name(type));
				} else {
					if (prev == NO_NODE) s->ast->nodes[init_idx].child = cast;
					else s->ast->nodes[prev].next_bro = cast;
					item = cast;
				}
			}
		}

		prev = item;
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
		if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC) {
			if (n->flags & NODE_FLAG_STATIC)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"static variables can only be declared inside a function");
			check_global_var_init(s, idx);
		}
		else if (n->type == NODE_FN_DEC) check_fn_dec(s, idx);
		else if (n->type == NODE_EXTERN_FN_DEC) check_extern_fn(s, idx);
		else if (n->type != NODE_ENUM_DEC && n->type != NODE_TYPE_DEC
				&& n->type != NODE_NEWTYPE_DEC && n->type != NODE_EXTERN_VAR_DEC
				&& n->type != NODE_STRUCT_DEC)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"only variable, function, enum and type declarations are allowed at the top level");
		idx = n->next_bro;
	}
}

static void decl_globals (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_FN_DEC || n->type == NODE_EXTERN_FN_DEC) {
			uint32_t ret = s->ast->nodes[n->child].next_bro;
			resolve_fn_types(s, idx);
			sema_declare(s, n->str, n->len, SYMBOL_FN,
					s->ast->nodes[ret].data_type, idx);
		} else if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC
				|| n->type == NODE_EXTERN_VAR_DEC) {
			resolve_type_node(s, n);
			check_storage_decl(s, n);
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

	decl_types(s);
	sema_resolve_types(s);
	resolve_enum_bases(s);
	sema_finalize_types(s);
	sema_layout_structs(s);
	decl_globals(s);
	bind_enums(s);

	check_root(s);
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
