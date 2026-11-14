#include "ir/ir.h"
#include "ir/internal.h"

#define ABI_REG_BYTES 16

typedef struct Leaf {
	uint32_t off;
	TypeId type;
} Leaf;

uint32_t struct_eightbytes (TypeId type)
{
	uint32_t size = types[type].size;

	if (type_kind(type) != TK_STRUCT || size > ABI_REG_BYTES) return 0;
	return (size + 7) >> 3;
}

static uint32_t collect_leaves (TypeId type, uint32_t base, Leaf *out, uint32_t n)
{
	const Type *t = types + type;

	for (uint16_t i = 0; i < t->field_count && n != UINT32_MAX; i++) {
		const StructField *f = t->fields + i;

		if (f->grp) return UINT32_MAX;
		if (type_kind(f->type) == TK_STRUCT) {
			n = collect_leaves(f->type, base + f->offset, out, n);
			continue;
		}
		if (((base + f->offset) & 7) + types[f->type].size > 8) return UINT32_MAX;
		out[n].off = base + f->offset;
		out[n].type = f->type;
		n++;
	}
	return n;
}

static uint32_t leaves_of (TypeId type, Leaf *out)
{
	if (!struct_is_small(type) || type_has_bitfield(type)) return 0;
	uint32_t n = collect_leaves(type, 0, out, 0);
	return n == UINT32_MAX ? 0 : n;
}

static uint32_t load_chunk (IR *ir, uint32_t addr, uint32_t off, uint32_t bytes)
{
	uint32_t acc = NO_REG;
	uint32_t shift = 0;

	while (bytes) {
		uint32_t w = bytes >= 8 ? 8 : bytes >= 4 ? 4 : bytes >= 2 ? 2 : 1;
		TypeId t = w == 8 ? TYPE_u64 : w == 4 ? TYPE_u32 : w == 2 ? TYPE_u16 : TYPE_u8;
		uint32_t v = make_ld_ptr(ir, add_offset(ir, addr, off + shift / 8), t);

		if (w < 8) v = make_op(ir, IR_EXTEND, v, NO_REG, TYPE_u64);
		if (shift) v = make_op(ir, IR_LS, v, make_const(ir, shift, TYPE_u64), TYPE_u64);
		acc = acc == NO_REG ? v : make_op(ir, IR_OR_A, acc, v, TYPE_u64);
		shift += 8 * w;
		bytes -= w;
	}
	return acc;
}

static void store_chunk (IR *ir, uint32_t addr, uint32_t v, uint32_t off, uint32_t bytes)
{
	uint32_t shift = 0;

	while (bytes) {
		uint32_t w = bytes >= 8 ? 8 : bytes >= 4 ? 4 : bytes >= 2 ? 2 : 1;
		TypeId t = w == 8 ? TYPE_u64 : w == 4 ? TYPE_u32 : w == 2 ? TYPE_u16 : TYPE_u8;
		uint32_t part = v;

		if (shift) part = make_op(ir, IR_RS, v, make_const(ir, shift, TYPE_u64), TYPE_u64);
		make_str_ptr(ir, add_offset(ir, addr, off + shift / 8), part, t);
		shift += 8 * w;
		bytes -= w;
	}
}

static uint32_t load_leaf (IR *ir, uint32_t addr, const Leaf *lf)
{
	uint32_t size = types[lf->type].size;
	TypeId uns = size == 1 ? TYPE_u8 : size == 2 ? TYPE_u16 : TYPE_u32;
	uint32_t v = make_ld_ptr(ir, add_offset(ir, addr, lf->off), lf->type);

	if (size == 8) return v;
	if (types[lf->type].sign) v = make_op(ir, IR_EXTEND, v, NO_REG, uns);
	return make_op(ir, IR_EXTEND, v, NO_REG, TYPE_u64);
}

static void store_leaf (IR *ir, uint32_t addr, uint32_t word, const Leaf *lf)
{
	uint32_t v = word;
	uint32_t bit = (lf->off & 7) * 8;

	if (bit) v = make_op(ir, IR_RS, v, make_const(ir, bit, TYPE_u64), TYPE_u64);
	if (types[lf->type].size < 8) v = make_op(ir, IR_EXTEND, v, NO_REG, lf->type);
	make_str_ptr(ir, add_offset(ir, addr, lf->off), v, lf->type);
}

void pack_struct (IR *ir, uint32_t addr, TypeId type, uint32_t *out)
{
	Leaf leaves[SROA_MAX_LEAVES];
	uint32_t size = types[type].size;
	uint32_t n = leaves_of(type, leaves);

	out[0] = NO_REG;
	out[1] = NO_REG;
	if (!n) {
		out[0] = load_chunk(ir, addr, 0, size > 8 ? 8 : size);
		if (size > 8) out[1] = load_chunk(ir, addr, 8, size - 8);
		return;
	}
	for (uint32_t i = 0; i < n; i++) {
		uint32_t w = leaves[i].off >> 3;
		uint32_t bit = (leaves[i].off & 7) * 8;
		uint32_t v = load_leaf(ir, addr, leaves + i);

		if (bit) v = make_op(ir, IR_LS, v, make_const(ir, bit, TYPE_u64), TYPE_u64);
		out[w] = out[w] == NO_REG ? v : make_op(ir, IR_OR_A, out[w], v, TYPE_u64);
	}
	for (uint32_t w = 0; w < struct_eightbytes(type); w++) {
		if (out[w] == NO_REG) out[w] = make_const(ir, 0, TYPE_u64);
	}
}

void unpack_struct (IR *ir, uint32_t addr, TypeId type, const uint32_t *in)
{
	Leaf leaves[SROA_MAX_LEAVES];
	uint32_t size = types[type].size;
	uint32_t n = leaves_of(type, leaves);

	if (!n) {
		store_chunk(ir, addr, in[0], 0, size > 8 ? 8 : size);
		if (size > 8) store_chunk(ir, addr, in[1], 8, size - 8);
		return;
	}
	for (uint32_t i = 0; i < n; i++)
		store_leaf(ir, addr, in[leaves[i].off >> 3], leaves + i);
}
