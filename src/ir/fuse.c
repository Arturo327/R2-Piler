#include "ir/ir.h"
#include "ir/internal.h"

#include <string.h>

#define FUSE_MAX_IMAGE 4096
#define FUSE_TEMPLATE_MIN 64
#define IMAGE_MAX (1u << 20)

enum {
	ST_BUSY = 0,
	ST_FREE,
	ST_CONST
};

typedef struct Fuse {
	IR *ir;
	uint8_t *img;
	uint8_t *st;
	uint32_t size;
	int skipped;
} Fuse;

static void put_value (Fuse *f, uint32_t idx, TypeId type, uint32_t off);

int const_tree (IR *ir, uint32_t idx)
{
	ASTNode *n = ir->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64: case NODE_LIT_CHAR: case NODE_LIT_STR:
		return 1;
	case NODE_CAST:
		return ir->ast->nodes[n->child].type == NODE_LIT_NULL;
	case NODE_ARRAY_INIT: case NODE_STRUCT_INIT: case NODE_FIELD_INIT:
		break;
	default:
		return 0;
	}
	for (uint32_t k = n->child; k != NO_NODE; k = ir->ast->nodes[k].next_bro) {
		if (!const_tree(ir, k)) return 0;
	}
	return 1;
}

static uint64_t fuse_bits (ASTNode *n)
{
	if (n->type == NODE_LIT_CHAR) return (uint64_t)(int64_t)n->chr;
	if (n->type == NODE_LIT_i64 || n->type == NODE_LIT_u64) return n->u64;
	return 0;
}

static void put_scalar (Fuse *f, uint32_t off, uint32_t size, uint64_t v)
{
	for (uint32_t k = 0; k < size; k++) {
		f->img[off + k] = (uint8_t)(v >> (8 * k));
		f->st[off + k] = ST_CONST;
	}
}

static void put_string (Fuse *f, ASTNode *lit, TypeId arr, uint32_t off)
{
	uint32_t count = types[arr].count;

	memset(f->img + off, 0, count);
	memcpy(f->img + off, lit->str, lit->len);
	memset(f->st + off, ST_CONST, count);
}

static void put_array (Fuse *f, uint32_t idx, TypeId arr, uint32_t off)
{
	ASTNode *nodes = f->ir->ast->nodes;
	TypeId elem = types[arr].base;
	uint32_t item = nodes[idx].child;

	while (item != NO_NODE) {
		put_value(f, item, elem, off);
		off += types[elem].size;
		item = nodes[item].next_bro;
	}
}

static void put_struct (Fuse *f, uint32_t init, TypeId type, uint32_t off)
{
	ASTNode *nodes = f->ir->ast->nodes;
	const Type *t = types + type;

	memset(f->st + off, ST_FREE, t->size);
	for (uint16_t i = 0; i < t->field_count; i++)
		memset(f->st + off + t->fields[i].offset, ST_BUSY,
				types[t->fields[i].type].size);

	for (uint32_t it = nodes[init].child; it != NO_NODE; it = nodes[it].next_bro) {
		ASTNode *fi = nodes + it;
		const StructField *sf = find_field(type, fi->str, fi->len);

		if (const_tree(f->ir, fi->child))
			put_value(f, fi->child, sf->type, off + sf->offset);
		else f->skipped = 1;
	}
}

static void put_value (Fuse *f, uint32_t idx, TypeId type, uint32_t off)
{
	ASTNode *n = f->ir->ast->nodes + idx;
	TypeKind kind = type_kind(type);

	if (n->type == NODE_LIT_STR) put_string(f, n, type, off);
	else if (kind == TK_STRUCT) put_struct(f, idx, type, off);
	else if (kind == TK_ARRAY) put_array(f, idx, type, off);
	else put_scalar(f, off, types[type].size, fuse_bits(n));
}

static int type_has_bitfield (TypeId id)
{
	const Type *t = types + id;

	if (type_kind(id) == TK_ARRAY || type_kind(id) == TK_SLICE)
		return type_has_bitfield(t->base);
	if (type_kind(id) != TK_STRUCT) return 0;

	for (uint16_t i = 0; i < t->field_count; i++) {
		if (t->fields[i].bits) return 1;
		if (type_has_bitfield(t->fields[i].type)) return 1;
	}
	return 0;
}

static void make_range (Fuse *f, uint32_t base, uint32_t from, uint32_t to)
{
	IR *ir = f->ir;

	while (from < to) {
		uint32_t left = to - from;
		uint32_t w = left >= 8 ? 8 : left >= 4 ? 4 : left >= 2 ? 2 : 1;
		TypeId t = w == 8 ? TYPE_u64 : w == 4 ? TYPE_u32 : w == 2 ? TYPE_u16 : TYPE_u8;

		uint64_t v = 0;
		for (uint32_t k = w; k-- > 0;)
			v = (v << 8) | f->img[from + k];

		uint32_t at = add_offset(ir, base, from);
		uint32_t c = make_const(ir, (int64_t)v, t);
		make_str_ptr(ir, at, c, t);
		from += w;
	}
}

static void make_runs (Fuse *f, uint32_t base)
{
	uint32_t i = 0;

	while (i < f->size) {
		uint32_t from = i;
		uint32_t to = i;

		if (f->st[i] != ST_CONST) {
			i++;
			continue;
		}
		while (i < f->size && f->st[i] != ST_BUSY) {
			to = f->st[i] == ST_CONST ? i + 1 : to;
			i++;
		}
		make_range(f, base, from, to);
	}
}

int fuse_struct_init (IR *ir, uint32_t base, ASTNode *init, TypeId type)
{
	Fuse f = { .ir = ir, .size = types[type].size };

	if (ir->scalarize || f.size > FUSE_MAX_IMAGE) return 0;
	if (type_has_bitfield(type)) return 0;
	f.img = arena_alloc(ir->arena, f.size);
	f.st = arena_alloc(ir->arena, f.size);
	memset(f.img, 0, f.size);
	memset(f.st, ST_BUSY, f.size);
	put_struct(&f, (uint32_t)(init - ir->ast->nodes), type, 0);

	if (!f.skipped && f.size > FUSE_TEMPLATE_MIN) {
		uint32_t src = make_blob(ir, f.img, f.size);

		gen_copy(ir, base, src, type);
		return 1;
	}
	make_runs(&f, base);
	return 1;
}

static int image_type_ok (TypeId id)
{
	TypeKind kind = type_kind(id);
	const Type *t = types + id;

	if (kind == TK_PTR || kind == TK_SLICE || kind == TK_STR) return 0;
	if (kind == TK_ARRAY) return image_type_ok(t->base);
	if (kind != TK_STRUCT) return 1;
	for (uint16_t i = 0; i < t->field_count; i++)
		if (!image_type_ok(t->fields[i].type)) return 0;
	return 1;
}

uint8_t *const_image (IR *ir, uint32_t init, TypeId type)
{
	Fuse f = { .ir = ir, .size = types[type].size };

	if (f.size > IMAGE_MAX || type_has_bitfield(type)) return NULL;
	if (!image_type_ok(type) || !const_tree(ir, init)) return NULL;
	f.img = arena_alloc(ir->arena, f.size);
	f.st = arena_alloc(ir->arena, f.size);
	memset(f.img, 0, f.size);
	memset(f.st, ST_BUSY, f.size);
	put_value(&f, init, type, 0);
	return f.img;
}
