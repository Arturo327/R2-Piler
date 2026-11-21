#include "sema/common.h"

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

static int check_array_init_head (Sema *s, TypeId arr, uint32_t init_idx, TypeId elem, uint32_t count)
{
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
		return 1;
	}
	if (init->type != NODE_ARRAY_INIT) {
		error_report(s->err, ERR_ERROR, node_loc(init),
				"array initializer must be a '{...}' list or a string literal");
		return 1;
	}
	if (type_dyn_rank(arr) > 0) {
		error_report(s->err, ERR_ERROR, node_loc(init),
				"an array type with '[]' dimensions cannot be initialized with '{...}'");
		return 1;
	}
	if (array_init_designated(s, init_idx)) {
		check_designated_init(s, arr, init_idx);
		return 1;
	}
	return 0;
}

void check_array_init_at (Sema *s, TypeId arr, uint32_t init_idx)
{
	TypeId elem = types[arr].base;
	uint32_t count = types[arr].count;
	ASTNode *init = s->ast->nodes + init_idx;

	if (check_array_init_head(s, arr, init_idx, elem, count)) return;
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

		if (n->data_type != TYPE_ERROR && type_ptr_repr(n->data_type) != TYPE_ERROR && init_type != TYPE_ERROR
				&& (type_ptr_repr(init_type) != TYPE_ERROR || init_type == TYPE_NULL
				|| init_type == TYPE_STR) && !is_const_ptr_init(s, n->child))
			error_report(s->err, ERR_ERROR, node_loc(n),
					"global pointer initializers only support NULL, a string literal or the address of a global");

		check_var_init_type(s, n, init_type);
	}
	s->init_node = saved;
	sym->state = INIT_DONE;
	record_init_order(s, idx);
}

