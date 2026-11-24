#include "sema/common.h"

static int ptr_base_ok (TypeId base)
{
	TypeKind kind = type_kind(base);

	return kind == TK_VOID || kind == TK_INT || kind == TK_ENUM || kind == TK_FN
			|| kind == TK_NEWTYPE || kind == TK_PTR || kind == TK_STRUCT;
}

static TypeId resolve_type_ref (Sema *s, ASTNode *ref, uint8_t *done, int *deferred)
{
	TypeId id = sema_lookup_type(s, ref->str, ref->len,
			(ref->flags & NODE_FLAG_IMPORT_BODY) != 0);

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
		params[i] = type_unconst_of(params[i]);
	}
	for (uint16_t i = 0; i < ret_count; i++) {
		if (resolve_fn_item(s, &item, rets + i, done, deferred, "return") == TYPE_ERROR)
			return TYPE_ERROR;
		rets[i] = type_unconst_of(rets[i]);
	}

	TypeId fn = type_fn_of(params, param_count, rets, ret_count,
			(ref->flags & NODE_FLAG_VARARG) != 0);

	if (fn == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
		return TYPE_ERROR;
	}
	return fn;
}

enum {
	SPEC_BARE_FN = 1,
	SPEC_PTR_BASE = 2
};

static TypeId resolve_spec_at (Sema *s, ASTNode *owner,
		uint8_t *done, int *deferred, int flags);

static TypeId resolve_const_spec (Sema *s, ASTNode *ref, uint8_t *done, int *deferred, int flags)
{
	TypeId base = ref->type_node != NO_TYPE_NODE
			? resolve_spec_at(s, ref, done, deferred, flags) : ref->data_type;

	if (deferred && *deferred) return TYPE_ERROR;
	if (base == TYPE_ERROR) return TYPE_ERROR;
	if ((base == TYPE_VOID && !(flags & SPEC_PTR_BASE))
			|| base == TYPE_ANY || type_kind(base) == TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"'const' cannot be applied to type %s;"
				" only object types can be const", types[base].name);
		return TYPE_ERROR;
	}

	TypeId id = type_const_of(base);

	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
		return TYPE_ERROR;
	}
	return id;
}

static TypeId resolve_spec_at (Sema *s, ASTNode *owner, uint8_t *done, int *deferred, int flags)
{
	if (owner->type_node == NO_TYPE_NODE) return owner->data_type;

	uint32_t ref_idx = (uint32_t)(owner - s->ast->nodes) + owner->type_node;
	ASTNode *ref = s->ast->nodes + ref_idx;

	if (ref->type == NODE_TYPE_CONST)
		return resolve_const_spec(s, ref, done, deferred, flags);
	if (ref->type == NODE_TYPE_ARRAY || ref->type == NODE_TYPE_SLICE)
		return resolve_array_spec(s, ref, done, deferred,
				ref->type == NODE_TYPE_SLICE);
	if (ref->type == NODE_TYPE_FN) {
		TypeId fn = resolve_fn_spec(s, ref, done, deferred);

		if (fn == TYPE_ERROR || (flags & SPEC_BARE_FN)) return fn;
		TypeId id = type_ptr_to(fn);

		if (id == TYPE_ERROR) {
			error_report(s->err, ERR_ERROR, node_loc(ref), "too many types");
			return TYPE_ERROR;
		}
		return id;
	}
	if (ref->type != NODE_TYPE_PTR)
		return resolve_type_ref(s, ref, done, deferred);

	TypeId id = resolve_spec_at(s, ref, done, deferred, SPEC_BARE_FN | SPEC_PTR_BASE);
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
