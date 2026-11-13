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

static void zero_fill (IR *ir, uint32_t base, uint32_t from, uint32_t count, TypeId elem)
{
	uint32_t loop = ir->label_count++;
	uint32_t end = ir->label_count++;
	uint32_t i = new_reg(ir, TYPE_i64);
	uint32_t zero = make_const(ir, 0, elem);
	uint32_t one = make_const(ir, 1, TYPE_i64);
	uint32_t limit = make_const(ir, count, TYPE_i64);

	make_const_into(ir, i, from, TYPE_i64);
	make_label(ir, loop);
	make_jump(ir, IR_JNZ, make_op(ir, IR_GE, i, limit, TYPE_i64), end);
	make_str_ptr(ir, make_op(ir, IR_ADD, base, i, TYPE_i64), zero, elem);
	make_move(ir, i, make_op(ir, IR_ADD, i, one, TYPE_i64), TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, loop);
	make_label(ir, end);
}

void gen_array_init (IR *ir, uint32_t base, uint32_t init_idx, TypeId arr, int zero)
{
	ASTNode *init = ir->ast->nodes + init_idx;
	TypeId elem = types[arr].base;
	uint32_t elem_size = types[elem].size;

	if (init->type == NODE_LIT_STR) {
		store_string(ir, base, init, elem);
		if (zero && (uint32_t)init->len + 1 < types[arr].count)
			zero_fill(ir, base, (uint32_t)init->len + 1, types[arr].count, elem);
		return;
	}

	uint32_t i = 0;
	uint32_t item = init->child;

	while (item != NO_NODE) {
		if (type_kind(elem) == TK_ARRAY) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_array_init(ir, at, item, elem, zero);
		} else if (type_kind(elem) == TK_STRUCT) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_struct_into(ir, at, item, elem, 1);
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

static void gen_copy_loop (IR *ir, uint32_t dst, uint32_t src, uint32_t size)
{
	uint32_t loop = ir->label_count++;
	uint32_t end = ir->label_count++;
	uint32_t off = new_reg(ir, TYPE_i64);
	uint32_t limit = make_const(ir, size, TYPE_i64);
	uint32_t step = make_const(ir, 8, TYPE_i64);

	make_const_into(ir, off, 0, TYPE_i64);
	make_label(ir, loop);
	make_jump(ir, IR_JNZ, make_op(ir, IR_GE, off, limit, TYPE_i64), end);

	uint32_t val = make_ld_ptr(ir, make_op(ir, IR_ADD, src, off, TYPE_i64), TYPE_u64);
	make_str_ptr(ir, make_op(ir, IR_ADD, dst, off, TYPE_i64), val, TYPE_u64);
	make_move(ir, off, make_op(ir, IR_ADD, off, step, TYPE_i64), TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, loop);
	make_label(ir, end);
}

static void gen_zero_loop (IR *ir, uint32_t base, uint64_t size)
{
	uint32_t loop = ir->label_count++;
	uint32_t end = ir->label_count++;
	uint32_t off = new_reg(ir, TYPE_i64);
	uint32_t limit = make_const(ir, size, TYPE_i64);
	uint32_t step = make_const(ir, 8, TYPE_i64);
	uint32_t zero = make_const(ir, 0, TYPE_u64);

	make_const_into(ir, off, 0, TYPE_i64);
	make_label(ir, loop);
	make_jump(ir, IR_JNZ, make_op(ir, IR_GE, off, limit, TYPE_i64), end);
	make_str_ptr(ir, make_op(ir, IR_ADD, base, off, TYPE_i64), zero, TYPE_u64);
	make_move(ir, off, make_op(ir, IR_ADD, off, step, TYPE_i64), TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, loop);
	make_label(ir, end);
}

static void gen_zero_range (IR *ir, uint32_t base, uint64_t size)
{
	uint64_t off = 0;

	if ((size >> 3) > COPY_UNROLL) {
		off = size & ~7u;
		gen_zero_loop(ir, base, off);
	}
	while (off < size) {
		uint64_t left = size - off;
		TypeId ut = left >= 8 ? TYPE_u64 : left >= 4 ? TYPE_u32
				: left >= 2 ? TYPE_u16 : TYPE_u8;

		make_str_ptr(ir, off ? add_offset(ir, base, off) : base, make_const(ir, 0, ut), ut);
		off += types[ut].size;
	}
}

static void gen_copy_fields (IR *ir, uint32_t dst, uint32_t src, TypeId type, uint32_t base)
{
	const Type *t = types + type;

	for (uint16_t i = 0; i < t->field_count; i++) {
		const StructField *f = t->fields + i;
		uint32_t off = base + f->offset;
		uint32_t val;

		if (type_kind(f->type) == TK_STRUCT) {
			gen_copy_fields(ir, dst, src, f->type, off);
			continue;
		}
		val = make_ld_ptr(ir, add_offset(ir, src, off), f->type);
		make_str_ptr(ir, add_offset(ir, dst, off), val, f->type);
	}
}

void gen_copy (IR *ir, uint32_t dst, uint32_t src, TypeId type)
{
	uint32_t size = types[type].size;
	uint32_t off = 0;
	int budget = SROA_MAX_LEAVES;

	if (type_scalar_leaves(type, &budget)) {
		gen_copy_fields(ir, dst, src, type, 0);
		return;
	}

	if ((size >> 3) > COPY_UNROLL) {
		off = size & ~7u;
		gen_copy_loop(ir, dst, src, off);
	}
	while (off < size) {
		uint32_t left = size - off;
		TypeId ut = left >= 8 ? TYPE_u64 : left >= 4 ? TYPE_u32
				: left >= 2 ? TYPE_u16 : TYPE_u8;
		uint32_t val = make_ld_ptr(ir, add_offset(ir, src, off), ut);

		make_str_ptr(ir, add_offset(ir, dst, off), val, ut);
		off += types[ut].size;
	}
}

static uint64_t bitfield_mask (const StructField *f)
{
	if (f->bits >= 64) return UINT64_MAX;
	return ((uint64_t)1 << f->bits) - 1;
}

uint32_t bitfield_extract (IR *ir, uint32_t raw, const StructField *f)
{
	TypeId holder = f->type;
	uint32_t width = 8u * types[holder].size;
	uint32_t v = raw;

	if (f->bit_off)
		v = make_op(ir, IR_RS, v, make_const(ir, f->bit_off, holder), holder);
	v = make_op(ir, IR_AND_A, v, make_const(ir, (int64_t)bitfield_mask(f), holder), holder);
	if (types[holder].sign && f->bits < width) {
		uint32_t shift = width - f->bits;
		v = make_op(ir, IR_LS, v, make_const(ir, shift, holder), holder);
		v = make_op(ir, IR_RS, v, make_const(ir, shift, holder), holder);
	}
	return v;
}

uint32_t gen_bitfield_store (IR *ir, uint32_t addr, const StructField *f, uint32_t value)
{
	TypeId holder = f->type;
	uint64_t mask = bitfield_mask(f);
	uint32_t old = make_ld_ptr(ir, addr, holder);
	uint32_t v = make_op(ir, IR_AND_A, value, make_const(ir, (int64_t)mask, holder), holder);

	if (f->bit_off)
		v = make_op(ir, IR_LS, v, make_const(ir, f->bit_off, holder), holder);
	old = make_op(ir, IR_AND_A, old,
			make_const(ir, (int64_t)~(mask << f->bit_off), holder), holder);
	v = make_op(ir, IR_OR_A, old, v, holder);
	make_str_ptr(ir, addr, v, holder);
	return bitfield_extract(ir, v, f);
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

	if (init->type == NODE_LIT_STR) return 0;
	for (uint32_t it = init->child; it != NO_NODE; it = ir->ast->nodes[it].next_bro) {
		if (init_needs_zero(ir, it, elem)) return 1;
	}
	return 0;
}

static int init_needs_zero (IR *ir, uint32_t idx, TypeId type)
{
	ASTNode *n = ir->ast->nodes + idx;
	const Type *t = types + type;

	if (type_kind(type) == TK_ARRAY) return arr_needs_zero(ir, n, type);
	if (type_kind(type) != TK_STRUCT || n->type != NODE_STRUCT_INIT) return 0;

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

		if (fused && const_tree(ir, fi->child)) {
			item = fi->next_bro;
			continue;
		}
		uint32_t at = add_offset(ir, base, f->offset);

		if (f->bits)
			gen_bitfield_store(ir, at, f, gen_expr(ir, ir->ast->nodes + fi->child));
		else if (type_kind(f->type) == TK_STRUCT)
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
	if (e->type == NODE_FN_CALL && (fresh || struct_eightbytes(type))
			&& ret_count(ir, call_ret_node(ir, e)) == 1) {
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
