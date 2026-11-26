#include "ir/ir.h"
#include "ir/internal.h"

#include <string.h>

#define COPY_UNROLL 8

static void store_string (IR *ir, uint32_t base, ASTNode *lit, TypeId elem)
{
	uint32_t zero = make_const(ir, 0, elem);

	for (uint16_t i = 0; i < lit->len; i++) {
		uint32_t c = make_const(ir, (uint8_t)lit->str[i], elem);

		make_str_ptr(ir, add_offset(ir, base, i), c, elem);
	}
	make_str_ptr(ir, add_offset(ir, base, lit->len), zero, elem);
}

static void gen_array_elem (IR *ir, uint32_t at, uint32_t item, TypeId elem, int zero)
{
	if (type_kind(elem) == TK_ARRAY) {
		gen_array_init(ir, at, item, elem, zero);
	} else if (type_is_struct_like(elem)) {
		gen_struct_into(ir, at, item, elem, 1);
	} else if (type_kind(elem) == TK_SLICE) {
		gen_slice_desc(ir, at, ir->ast->nodes + item, elem);
	} else {
		uint32_t val = gen_expr(ir, ir->ast->nodes + item);

		make_str_ptr(ir, at, val, elem);
	}
}

static uint32_t designated_addr (IR *ir, uint32_t base, ASTNode *item, TypeId arr, TypeId *target)
{
	TypeId level = arr;
	uint32_t node = item->child;

	for (uint32_t k = 0; k < item->sym; k++) {
		ASTNode *dim = ir->ast->nodes + node;
		TypeId elem = types[level].base;

		base = add_offset(ir, base, dim->u64 * types[elem].size);
		level = elem;
		node = dim->next_bro;
	}
	*target = level;
	return base;
}

void gen_array_init (IR *ir, uint32_t base, uint32_t init_idx, TypeId arr, int zero)
{
	ASTNode *init = ir->ast->nodes + init_idx;
	TypeId elem = types[arr].base;
	uint32_t elem_size = types[elem].size;
	uint32_t item = init->child;

	if (init->type == NODE_LIT_STR) {
		store_string(ir, base, init, elem);
		if (zero && (uint32_t)init->len + 1 < types[arr].count)
			zero_fill(ir, base, (uint32_t)init->len + 1, types[arr].count, elem);
		return;
	}
	if (item != NO_NODE && ir->ast->nodes[item].type == NODE_INDEX_INIT) {
		if (zero) gen_zero_range(ir, base, types[arr].size);
		while (item != NO_NODE) {
			TypeId target;
			uint32_t at = designated_addr(ir, base, ir->ast->nodes + item, arr, &target);
			uint32_t value = (uint32_t)ir->ast->nodes[item].u64;

			gen_array_elem(ir, at, value, target, 0);
			item = ir->ast->nodes[item].next_bro;
		}
		return;
	}

	uint32_t i = 0;

	while (item != NO_NODE) {
		if (type_kind(elem) == TK_ARRAY) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_array_init(ir, at, item, elem, zero);
		} else if (type_is_struct_like(elem)) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_struct_into(ir, at, item, elem, 1);
		} else if (type_kind(elem) == TK_SLICE) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_slice_desc(ir, at, ir->ast->nodes + item, elem);
		} else {
			uint32_t val = gen_expr(ir, ir->ast->nodes + item);

			make_str_ptr(ir, add_offset(ir, base, (uint64_t)i * elem_size), val, elem);
		}
		item = ir->ast->nodes[item].next_bro;
		i++;
	}
}

static uint32_t pooled_temp (IR *ir, TypeId type)
{
	TempSlot *t;

	for (uint32_t i = 0; i < ir->temp_count; i++) {
		t = ir->temps + i;
		if (t->busy || t->type != type) continue;
		t->busy = 1;
		return t->reg;
	}
	if (ir->temp_count >= ir->temp_cap) {
		uint32_t old = ir->temp_cap;

		ir->temp_cap = old ? old << 1 : 8;
		ir->temps = arena_realloc(ir->arena, ir->temps,
				old * sizeof(TempSlot), ir->temp_cap * sizeof(TempSlot));
	}
	t = ir->temps + ir->temp_count++;
	t->reg = new_reg(ir, type);
	t->type = type;
	t->busy = 1;
	ir->addressed[t->reg] = 1;
	return t->reg;
}

uint32_t new_struct_temp (IR *ir, TypeId type)
{
	uint32_t reg = pooled_temp(ir, type);
	uint32_t dst = new_reg(ir, TYPE_i64);

	ir->no_tail = 1;
	make_addr_local(ir, dst, reg, type);
	return dst;
}


uint32_t gen_slice_desc (IR *ir, uint32_t dst, ASTNode *val, TypeId type)
{
	TypeId ptr_type = type_ptr_to(type_flat_element(type));

	if (val->type == NODE_FN_CALL && type_dyn_rank(val->data_type) > 0) {
		uint32_t buf = gen_call_desc(ir, val);

		gen_copy(ir, dst, buf, val->data_type);
		return make_ld_ptr(ir, buf, ptr_type);
	}

	uint32_t src_ptr = gen_expr(ir, val);
	make_str_ptr(ir, dst, src_ptr, ptr_type);

	int slice_index = 0;
	TypeId level_type = type;
	int level = 0;

	while (type_kind(level_type) == TK_ARRAY || type_kind(level_type) == TK_SLICE) {
		if (type_kind(level_type) == TK_SLICE) {
			uint32_t length = slice_dim_reg(ir, val, level, TYPE_i64);
			make_str_ptr(ir, add_offset(ir, dst, (uint64_t)8 * (slice_index + 1)), length, TYPE_i64);
			slice_index++;
		}
		level_type = types[level_type].base;
		level++;
	}
	return src_ptr;
}

static uint32_t init_find (IR *ir, ASTNode *init, const char *name, uint16_t len)
{
	for (uint32_t it = init->child; it != NO_NODE; it = ir->ast->nodes[it].next_bro) {
		ASTNode *fi = ir->ast->nodes + it;
		if (fi->len == len && memcmp(fi->str, name, len) == 0) return fi->child;
	}
	return NO_NODE;
}

static int init_needs_zero (IR *ir, uint32_t idx, TypeId type);

static int alt_needs_zero (IR *ir, ASTNode *init, const Type *t, const StructField *f)
{
	for (uint32_t j = 0; j < t->field_count; j++) {
		const StructField *g = t->fields + j;
		uint32_t val;

		if (g->grp != f->grp || g->alt != f->alt) continue;
		val = init_find(ir, init, g->name, g->len);
		if (val == NO_NODE || init_needs_zero(ir, val, g->type)) return 1;
	}
	return 0;
}

static int group_needs_zero (IR *ir, ASTNode *init, const Type *t, uint16_t grp)
{
	for (uint32_t k = 0; k < t->field_count; k++) {
		const StructField *f = t->fields + k;

		if (f->grp != grp) continue;
		if (init_find(ir, init, f->name, f->len) != NO_NODE)
			return alt_needs_zero(ir, init, t, f);
	}
	return 1;
}

static int arr_needs_zero (IR *ir, ASTNode *init, TypeId type)
{
	TypeId elem = types[type].base;
	uint32_t item = init->child;

	if (init->type == NODE_LIT_STR) return 0;
	if (item != NO_NODE && ir->ast->nodes[item].type == NODE_INDEX_INIT) return 1;
	for (; item != NO_NODE; item = ir->ast->nodes[item].next_bro) {
		if (init_needs_zero(ir, item, elem)) return 1;
	}
	return 0;
}

static int init_needs_zero (IR *ir, uint32_t idx, TypeId type)
{
	ASTNode *n = ir->ast->nodes + idx;
	const Type *t = types + type;

	if (type_kind(type) == TK_ARRAY) return arr_needs_zero(ir, n, type);
	if (!type_is_struct_like(type) || n->type != NODE_STRUCT_INIT) return 0;

	for (uint32_t k = 0; k < t->field_count; k++) {
		const StructField *f = t->fields + k;

		if (f->grp) {
			int done = 0;

			for (uint32_t j = 0; j < k; j++)
				if (t->fields[j].grp == f->grp) done = 1;
			if (done) continue;
			if (group_needs_zero(ir, n, t, f->grp)) return 1;
			continue;
		}
		uint32_t val = init_find(ir, n, f->name, f->len);
		if (val == NO_NODE) return 1;
		if (init_needs_zero(ir, val, f->type)) return 1;
	}
	return 0;
}

void gen_struct_init (IR *ir, uint32_t base, ASTNode *init, TypeId type)
{
	if (init_needs_zero(ir, (uint32_t)(init - ir->ast->nodes), type))
		gen_zero_range(ir, base, types[type].size);

	int fused = fuse_struct_init(ir, base, init, type);
	uint32_t item = init->child;

	while (item != NO_NODE) {
		ASTNode *fi = ir->ast->nodes + item;
		const StructField *f = find_field(type, fi->str, fi->len);

		if (fused && const_tree(ir, fi->child) && type_kind(f->type) != TK_SLICE) {
			item = fi->next_bro;
			continue;
		}
		uint32_t at = add_offset(ir, base, f->offset);

		if (f->bits)
			gen_bitfield_store(ir, at, f, gen_expr(ir, ir->ast->nodes + fi->child));
		else if (type_is_struct_like(f->type))
			gen_struct_into(ir, at, fi->child, f->type, 1);
		else if (type_dyn_rank(f->type) > 0) {
			ASTNode *val = ir->ast->nodes + fi->child;
			gen_slice_desc(ir, at, val, f->type);
		} else if (type_kind(f->type) == TK_ARRAY)
			gen_array_init(ir, at, fi->child, f->type, 1);
		else make_str_ptr(ir, at, gen_expr(ir, ir->ast->nodes + fi->child), f->type);
		item = fi->next_bro;
	}
}

void gen_struct_into (IR *ir, uint32_t dst, uint32_t expr, TypeId type, int fresh)
{
	ASTNode *e = ir->ast->nodes + expr;

	if (e->type == NODE_CAST && (e->flags & NODE_FLAG_LITERAL) && e->child != NO_NODE) {
		expr = e->child;
		e = ir->ast->nodes + expr;
	}
	if (e->type == NODE_STRUCT_INIT) {
		if (!fresh) {
			uint32_t tmp = new_struct_temp(ir, type);
			gen_struct_init(ir, tmp, e, type);
			gen_copy(ir, dst, tmp, type);
			return;
		}
		gen_struct_init(ir, dst, e, type);
		return;
	}
	if (e->type == NODE_CTOR_CALL) {
		if (!fresh) {
			uint32_t tmp = new_struct_temp(ir, type);

			gen_ctor_call(ir, e, tmp);
			gen_copy(ir, dst, tmp, type);
			return;
		}
		gen_ctor_call(ir, e, dst);
		return;
	}
	if (e->type == NODE_FN_CALL && (fresh || struct_eightbytes(type))
			&& call_ret_count(ir, e) == 1) {
		gen_call(ir, e, dst);
		return;
	}
	gen_copy(ir, dst, gen_expr(ir, e), type);
}

int struct_is_small (TypeId type)
{
	int budget = SROA_MAX_LEAVES;
	return type_scalar_leaves(type, &budget);
}
