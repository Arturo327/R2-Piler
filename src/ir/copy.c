#include "ir/ir.h"
#include "ir/internal.h"

#include <string.h>

#define COPY_UNROLL 8

void zero_fill (IR *ir, uint32_t base, uint32_t from, uint32_t count, TypeId elem)
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

void gen_zero_range (IR *ir, uint32_t base, uint64_t size)
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

		if (type_is_struct_like(f->type)) {
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
