#include "ir/ir.h"
#include "ir/internal.h"

uint32_t gen_class_equal (IR *ir, uint32_t a, uint32_t b, TypeId type);

static uint32_t and_eq (IR *ir, uint32_t acc, uint32_t eq)
{
	if (acc == NO_REG) return eq;
	return make_op(ir, IR_AND_A, acc, eq, TYPE_i64);
}

static uint32_t gen_equal_bitfield (IR *ir, uint32_t a, uint32_t b,
		const StructField *f)
{
	return make_op(ir, IR_EQ,
			bitfield_extract(ir, make_ld_ptr(ir, a, f->type), f),
			bitfield_extract(ir, make_ld_ptr(ir, b, f->type), f), TYPE_i64);
}

static uint32_t gen_equal_fields (IR *ir, uint32_t a, uint32_t b, TypeId t)
{
	const StructField *fields = types[t].fields;
	uint32_t acc = NO_REG;

	for (uint16_t i = 0; i < types[t].field_count; i++) {
		const StructField *f = fields + i;
		uint32_t fa = add_offset(ir, a, f->offset);
		uint32_t fb = add_offset(ir, b, f->offset);
		uint32_t eq = f->bits ? gen_equal_bitfield(ir, fa, fb, f)
			: gen_class_equal(ir, fa, fb, f->type);

		acc = and_eq(ir, acc, eq);
	}
	return acc == NO_REG ? make_const(ir, 1, TYPE_i64) : acc;
}

static uint32_t gen_equal_array (IR *ir, uint32_t a, uint32_t b, TypeId t)
{
	TypeId elem = types[t].base;
	uint64_t stride = types[elem].size;
	uint32_t acc = NO_REG;

	for (uint32_t i = 0; i < types[t].count; i++) {
		uint32_t eq = gen_class_equal(ir, add_offset(ir, a, i * stride),
				add_offset(ir, b, i * stride), elem);

		acc = and_eq(ir, acc, eq);
	}
	return acc == NO_REG ? make_const(ir, 1, TYPE_i64) : acc;
}

static uint32_t gen_equal_slice (IR *ir, uint32_t a, uint32_t b, TypeId t)
{
	TypeId ptr = type_ptr_to(type_flat_element(t));
	uint32_t acc = make_op(ir, IR_EQ, make_ld_ptr(ir, a, ptr),
			make_ld_ptr(ir, b, ptr), TYPE_i64);
	int rank = type_dyn_rank(t);

	for (int d = 0; d < rank; d++) {
		uint64_t off = (uint64_t)(d + 1) * 8;
		uint32_t eq = make_op(ir, IR_EQ,
				make_ld_ptr(ir, add_offset(ir, a, off), TYPE_i64),
				make_ld_ptr(ir, add_offset(ir, b, off), TYPE_i64), TYPE_i64);

		acc = and_eq(ir, acc, eq);
	}
	return acc;
}

uint32_t gen_class_equal (IR *ir, uint32_t a, uint32_t b, TypeId type)
{
	TypeId t = type_unconst_of(type);
	TypeKind kind = type_kind(t);

	if (kind == TK_STRUCT || kind == TK_CLASS)
		return gen_equal_fields(ir, a, b, t);
	if (kind == TK_ARRAY)
		return gen_equal_array(ir, a, b, t);
	if (kind == TK_SLICE)
		return gen_equal_slice(ir, a, b, t);
	return make_op(ir, IR_EQ, make_ld_ptr(ir, a, t),
			make_ld_ptr(ir, b, t), TYPE_i64);
}
