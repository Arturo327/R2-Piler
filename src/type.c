#include <string.h>

#include "type.h"
#include "arena/arena.h"

Type types[TYPE_MAX];
uint16_t type_count;

static Arena type_arena;
static int type_arena_ready;

static const Type builtin_types[TYPE_COUNT] = {
	[TYPE_VOID] = { "void", 0, 0, 0 },
	[TYPE_i8] = { "i8", 0, 1, 1 },
	[TYPE_u8] = { "u8", 0, 1, 0 },
	[TYPE_i16] = { "i16", 0, 2, 1 },
	[TYPE_u16] = { "u16", 0, 2, 0 },
	[TYPE_i32] = { "i32", 0, 4, 1 },
	[TYPE_u32] = { "u32", 0, 4, 0 },
	[TYPE_i64] = { "i64", 0, 8, 1 },
	[TYPE_u64] = { "u64", 0, 8, 0 },
	[TYPE_UNTYPED_INT] = { "untyped int", 0, 8, 1 },
	[TYPE_UNTYPED_UINT] = { "untyped uint", 0, 8, 0 },
	[TYPE_UNTYPED_CHAR] = { "untyped char", 0, 1, 1 },
	[TYPE_ERROR] = { "error", 0, 0, 0 }
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
		types[i].kind = TYPE_KIND_PRIMITIVE;
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
	t->sign = types[base].sign;
	t->kind = (uint8_t)kind;
	t->builtin = 0;
	t->base = base;
	t->variant_count = 0;
	t->variants = NULL;
	return id;
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
	if (!is_enum(id)) return NULL;

	for (uint16_t i = 0; i < types[id].variant_count; i++) {
		EnumVariant *v = types[id].variants + i;
		if (v->len == len && memcmp(v->name, name, len) == 0) return v;
	}
	return NULL;
}

int is_enum (TypeId id)
{
	return id >= TYPE_COUNT && id < type_count
			&& types[id].kind == TYPE_KIND_ENUM;
}

TypeId type_base (TypeId id)
{
	if (id >= type_count) return TYPE_ERROR;
	return types[id].base;
}

int type_is_integer (TypeId id)
{
	return id >= TYPE_i8 && id <= TYPE_u64;
}

int type_is_numeric (TypeId id)
{
	return type_is_integer(id) || is_enum(id);
}
