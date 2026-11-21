#include "ir/ir.h"
#include "ir/internal.h"

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

