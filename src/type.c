#include <string.h>

#include "type.h"
#include "arena/arena.h"

Type types[TYPE_MAX];
uint16_t type_count;

static Arena type_arena;
static int type_arena_ready;

static const Type builtin_types[TYPE_COUNT] = {
	[TYPE_VOID] = { .name = "void", .size = 0, .sign = 0 },
	[TYPE_i8] = { .name = "i8", .size = 1, .sign = 1 },
	[TYPE_u8] = { .name = "u8", .size = 1, .sign = 0 },
	[TYPE_i16] = { .name = "i16", .size = 2, .sign = 1 },
	[TYPE_u16] = { .name = "u16", .size = 2, .sign = 0 },
	[TYPE_i32] = { .name = "i32", .size = 4, .sign = 1 },
	[TYPE_u32] = { .name = "u32", .size = 4, .sign = 0 },
	[TYPE_i64] = { .name = "i64", .size = 8, .sign = 1 },
	[TYPE_u64] = { .name = "u64", .size = 8, .sign = 0 },
	[TYPE_UNTYPED_INT] = { .name = "untyped int", .size = 8, .sign = 1 },
	[TYPE_UNTYPED_UINT] = { .name = "untyped uint", .size = 8, .sign = 0 },
	[TYPE_UNTYPED_CHAR] = { .name = "untyped char", .size = 1, .sign = 1 },
	[TYPE_ERROR] = { .name = "error", .size = 0, .sign = 0 },
	[TYPE_NULL] = { .name = "nil", .size = 8, .sign = 0 },
	[TYPE_STR] = { .name = "string", .size = 8, .sign = 0 }
};

static const uint8_t builtin_kind[TYPE_COUNT] = {
	[TYPE_VOID] = TK_VOID,
	[TYPE_i8] = TK_INT, [TYPE_u8] = TK_INT,
	[TYPE_i16] = TK_INT, [TYPE_u16] = TK_INT,
	[TYPE_i32] = TK_INT, [TYPE_u32] = TK_INT,
	[TYPE_i64] = TK_INT, [TYPE_u64] = TK_INT,
	[TYPE_UNTYPED_INT] = TK_UNTYPED_INT,
	[TYPE_UNTYPED_UINT] = TK_UNTYPED_INT,
	[TYPE_UNTYPED_CHAR] = TK_UNTYPED_INT,
	[TYPE_ERROR] = TK_ERROR,
	[TYPE_NULL] = TK_NULL,
	[TYPE_STR] = TK_STR
};

void init_types (void)
{
	if (type_arena_ready) arena_destroy(&type_arena);
	arena_init(&type_arena);
	type_arena_ready = 1;

	memset(types, 0, sizeof(types));
	for (int i = 0; i < TYPE_COUNT; i++) {
		types[i] = builtin_types[i];
		types[i].len = (uint16_t)strlen(types[i].name);
		types[i].kind = builtin_kind[i];
		types[i].builtin = 1;
		types[i].base = (TypeId)i;
	}
	type_count = TYPE_COUNT;
}

void types_destroy (void)
{
	if (!type_arena_ready) return;

	arena_destroy(&type_arena);
	type_arena_ready = 0;
	type_count = 0;
	memset(types, 0, sizeof(types));
}

static const char *copy_name (const char *name, uint16_t len)
{
	char *copy = arena_alloc(&type_arena, (size_t)len + 1);

	memcpy(copy, name, len);
	copy[len] = '\0';
	return copy;
}

TypeId add_type (const char *name, uint16_t len, TypeKind kind, TypeId base)
{
	if (type_count >= TYPE_MAX || base >= type_count) return TYPE_ERROR;

	TypeId id = type_count++;
	Type *t = types + id;

	t->name = copy_name(name, len);
	t->len = len;
	t->size = types[base].size;
	t->count = 0;
	t->sign = types[base].sign;
	t->kind = (uint8_t)kind;
	t->builtin = 0;
	t->base = base;
	t->variant_count = 0;
	t->variants = NULL;
	return id;
}

TypeId type_ptr_to (TypeId base)
{
	if (base == TYPE_ERROR || base >= type_count) return TYPE_ERROR;

	for (TypeId id = TYPE_COUNT; id < type_count; id++) {
		if (types[id].kind != TK_PTR || types[id].base != base) continue;
		return id;
	}

	uint32_t total = (uint32_t)types[base].len + 4;
	if (total > UINT16_MAX) return TYPE_ERROR;

	char *name = arena_alloc(&type_arena, (size_t)total + 1);
	memcpy(name, "ptr ", 4);
	memcpy(name + 4, types[base].name, types[base].len);
	name[total] = '\0';

	TypeId id = add_type(name, (uint16_t)total, TK_PTR, base);
	if (id == TYPE_ERROR) return TYPE_ERROR;
	types[id].size = 8;
	types[id].sign = 0;
	return id;
}

static uint16_t element_name_len (const Type *t)
{
	for (uint16_t i = 0; i < t->len; i++)
		if (t->name[i] == '[') return i;
	return t->len;
}

static uint16_t decimal_len (uint32_t value)
{
	uint16_t len = 1;

	while (value >= 10) {
		value /= 10;
		len++;
	}
	return len;
}

static char *array_name (Type *base, uint32_t count, uint16_t *out_len)
{
	uint16_t leaf = element_name_len(base);
	uint16_t digits = decimal_len(count);
	uint32_t total = (uint32_t)base->len + digits + 2;

	if (total > UINT16_MAX) return NULL;

	char *name = arena_alloc(&type_arena, (size_t)total + 1);
	uint32_t at = 0;

	memcpy(name, base->name, leaf);
	at += leaf;
	name[at++] = '[';
	for (uint16_t i = 0; i < digits; i++)
		name[at + digits - 1 - i] = (char)('0' + count % 10), count /= 10;
	at += digits;
	name[at++] = ']';
	memcpy(name + at, base->name + leaf, base->len - leaf);
	at += base->len - leaf;
	name[at] = '\0';
	*out_len = (uint16_t)at;
	return name;
}

TypeId type_array_of (TypeId base, uint32_t count)
{
	if (base == TYPE_ERROR || base >= type_count) return TYPE_ERROR;
	if (!count || !types[base].size) return TYPE_ERROR;

	for (TypeId id = TYPE_COUNT; id < type_count; id++) {
		if (types[id].kind != TK_ARRAY || types[id].base != base) continue;
		if (types[id].count == count) return id;
	}

	uint64_t total = (uint64_t)types[base].size * count;
	if (total > UINT32_MAX) return TYPE_ERROR;

	uint16_t len;
	char *name = array_name(types + base, count, &len);
	if (!name) return TYPE_ERROR;

	TypeId id = add_type(name, len, TK_ARRAY, base);
	if (id == TYPE_ERROR) return TYPE_ERROR;
	types[id].size = (uint32_t)total;
	types[id].count = count;
	types[id].sign = 0;
	return id;
}

TypeId type_slice_of (TypeId base)
{
	if (base == TYPE_ERROR || base >= type_count || !types[base].size)
		return TYPE_ERROR;

	for (TypeId id = TYPE_COUNT; id < type_count; id++) {
		if (types[id].kind == TK_SLICE && types[id].base == base) return id;
	}

	uint16_t leaf = element_name_len(types + base);
	uint32_t total = (uint32_t)types[base].len + 2;
	if (total > UINT16_MAX) return TYPE_ERROR;

	char *name = arena_alloc(&type_arena, (size_t)total + 1);
	memcpy(name, types[base].name, leaf);
	memcpy(name + leaf, "[]", 2);
	memcpy(name + leaf + 2, types[base].name + leaf, types[base].len - leaf);
	name[total] = '\0';

	TypeId id = add_type(name, (uint16_t)total, TK_SLICE, base);
	if (id == TYPE_ERROR) return TYPE_ERROR;
	types[id].size = 8u * (uint32_t)(2 + type_slice_rank(base));
	types[id].count = 0;
	types[id].sign = 0;
	return id;
}

TypeId type_flat_element (TypeId id)
{
	for (int i = 0; i < TYPE_MAX; i++) {
		TypeKind kind = type_kind(id);

		if (kind != TK_ARRAY && kind != TK_SLICE) return id;
		id = types[id].base;
	}
	return TYPE_ERROR;
}

int type_array_rank (TypeId id)
{
	int rank = 0;

	while (type_kind(id) == TK_ARRAY) {
		rank++;
		id = types[id].base;
	}
	return rank;
}

int type_slice_rank (TypeId id)
{
	int rank = 0;

	while (type_kind(id) == TK_SLICE) {
		rank++;
		id = types[id].base;
	}
	return rank;
}

TypeId find_type (const char *name, uint16_t len)
{
	for (TypeId id = TYPE_COUNT; id < type_count; id++) {
		if (types[id].len != len) continue;
		if (memcmp(types[id].name, name, len) == 0) return id;
	}
	return TYPE_ERROR;
}

int bind_enum_type (TypeId id, const EnumVariant *vars, uint16_t count)
{
	if (id < TYPE_COUNT || id >= type_count) return 0;

	EnumVariant *copy = arena_alloc(&type_arena, sizeof(EnumVariant) * count);
	for (uint16_t i = 0; i < count; i++) {
		copy[i].name = copy_name(vars[i].name, vars[i].len);
		copy[i].value = vars[i].value;
		copy[i].len = vars[i].len;
	}

	types[id].variants = copy;
	types[id].variant_count = count;
	return 1;
}

EnumVariant *find_variant (TypeId id, const char *name, uint16_t len)
{
	if (type_kind(id) != TK_ENUM) return NULL;

	for (uint16_t i = 0; i < types[id].variant_count; i++) {
		EnumVariant *v = types[id].variants + i;
		if (v->len == len && memcmp(v->name, name, len) == 0) return v;
	}
	return NULL;
}

TypeKind type_kind (TypeId id)
{
	if (id >= type_count) return TK_INVALID;
	return (TypeKind)types[id].kind;
}

int type_is_nominal (TypeId id)
{
	TypeKind kind = type_kind(id);

	return kind == TK_ENUM || kind == TK_NEWTYPE;
}

TypeId type_canon (TypeId id)
{
	if (type_kind(id) == TK_ALIAS) return types[id].base;
	return id;
}

TypeId type_repr (TypeId id)
{
	for (int i = 0; i < TYPE_MAX; i++) {
		TypeKind kind = type_kind(id);

		if (kind != TK_ALIAS && kind != TK_NEWTYPE) return id;
		id = types[id].base;
	}
	return TYPE_ERROR;
}
