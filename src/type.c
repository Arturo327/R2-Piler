#include <string.h>

#include "type.h"
#include "arena/arena.h"

Type types[TYPE_MAX];
uint16_t type_count;

#define TYPE_HASH_SIZE (TYPE_MAX * 2)

static uint16_t derived_tab[TYPE_HASH_SIZE];
static uint16_t name_tab[TYPE_HASH_SIZE];

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
	[TYPE_STR] = { .name = "string", .size = 8, .sign = 0 },
	[TYPE_ANY] = { .name = "any", .size = 16, .sign = 0 }
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
	[TYPE_STR] = TK_STR,
	[TYPE_ANY] = TK_STRUCT
};

static StructField any_fields[2];

static uint16_t *derived_slot (uint8_t kind, TypeId base, uint32_t count)
{
	uint32_t h = 2166136261u;

	h = (h ^ kind) * 16777619u;
	h = (h ^ base) * 16777619u;
	h = (h ^ count) * 16777619u;
	h ^= h >> 15;
	h &= TYPE_HASH_SIZE - 1;
	while (derived_tab[h]) {
		const Type *t = types + derived_tab[h];

		if (t->kind == kind && t->base == base && t->count == count) break;
		h = (h + 1) & (TYPE_HASH_SIZE - 1);
	}
	return derived_tab + h;
}

static uint16_t *name_slot (const char *name, uint16_t len)
{
	uint32_t h = 2166136261u;

	for (uint16_t i = 0; i < len; i++)
		h = (h ^ (uint8_t)name[i]) * 16777619u;
	h ^= h >> 15;
	h &= TYPE_HASH_SIZE - 1;
	while (name_tab[h]) {
		const Type *t = types + name_tab[h];

		if (t->len == len && memcmp(t->name, name, len) == 0) break;
		h = (h + 1) & (TYPE_HASH_SIZE - 1);
	}
	return name_tab + h;
}

void init_types (void)
{
	if (type_arena_ready) arena_destroy(&type_arena);
	arena_init(&type_arena);
	type_arena_ready = 1;

	memset(types, 0, sizeof(types));
	memset(derived_tab, 0, sizeof(derived_tab));
	memset(name_tab, 0, sizeof(name_tab));
	for (int i = 0; i < TYPE_COUNT; i++) {
		types[i] = builtin_types[i];
		types[i].len = (uint16_t)strlen(types[i].name);
		types[i].kind = builtin_kind[i];
		types[i].builtin = 1;
		types[i].base = (TypeId)i;
	}
	type_count = TYPE_COUNT;

	any_fields[0] = (StructField){ .name = "tag", .len = 3,
			.type = type_ptr_to(TYPE_i8), .offset = 0 };
	any_fields[1] = (StructField){ .name = "value", .len = 5,
			.type = TYPE_i64, .offset = 8 };
	types[TYPE_ANY].fields = any_fields;
	types[TYPE_ANY].field_count = 2;
	types[TYPE_ANY].align = 8;
	types[TYPE_ANY].layout = 2;
}

void types_destroy (void)
{
	if (!type_arena_ready) return;

	arena_destroy(&type_arena);
	type_arena_ready = 0;
	type_count = 0;
	memset(types, 0, sizeof(types));
	memset(derived_tab, 0, sizeof(derived_tab));
	memset(name_tab, 0, sizeof(name_tab));
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
	t->used = 0;
	t->exposed = 0;
	t->base = base;
	t->variant_count = 0;
	t->variants = NULL;
	t->fields = NULL;
	t->field_count = 0;
	t->align = 1;
	t->decl = 0;
	t->layout = 0;
	t->is_union = 0;
	t->is_packed = 0;
	t->params = NULL;
	t->rets = NULL;
	t->param_count = 0;
	t->ret_count = 0;
	t->fn_variadic = 0;

	if (kind == TK_ENUM || kind == TK_ALIAS || kind == TK_NEWTYPE || kind == TK_STRUCT) {
		uint16_t *slot = name_slot(name, len);
		*slot = id;
	}
	return id;
}

TypeId type_ptr_to (TypeId base)
{
	if (base == TYPE_ERROR || base >= type_count) return TYPE_ERROR;

	uint16_t *slot = derived_slot(TK_PTR, base, 0);
	if (*slot) return *slot;

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
	*slot = id;
	return id;
}

static uint32_t fn_name_len (const TypeId *params, uint16_t param_count,
		const TypeId *rets, uint16_t ret_count, int variadic)
{
	uint64_t total = 4;

	for (uint16_t i = 0; i < param_count; i++)
		total += (uint64_t)types[params[i]].len + 2;
	if (variadic) total += 5;
	if (ret_count == 1) total += (uint64_t)types[rets[0]].len + 3;
	else if (ret_count > 1) {
		total += 4;
		for (uint16_t i = 0; i < ret_count; i++)
			total += (uint64_t)types[rets[i]].len + 2;
	}
	return total > UINT16_MAX ? 0 : (uint32_t)total;
}

static char *put_name (char *at, const char *src, uint32_t len)
{
	memcpy(at, src, len);
	return at + len;
}

static char *fn_name (const TypeId *params, uint16_t param_count,
		const TypeId *rets, uint16_t ret_count, int variadic, uint16_t *out_len)
{
	uint32_t total = fn_name_len(params, param_count, rets, ret_count, variadic);

	if (!total) return NULL;

	char *name = arena_alloc(&type_arena, (size_t)total + 1);
	char *at = put_name(name, "fn(", 3);

	for (uint16_t i = 0; i < param_count; i++) {
		if (i) at = put_name(at, ", ", 2);
		at = put_name(at, types[params[i]].name, types[params[i]].len);
	}
	if (variadic) at = put_name(at, param_count ? ", ..." : "...", param_count ? 5 : 3);
	*at++ = ')';
	if (ret_count == 1) {
		at = put_name(at, " : ", 3);
		at = put_name(at, types[rets[0]].name, types[rets[0]].len);
	} else if (ret_count > 1) {
		at = put_name(at, " : (", 4);
		for (uint16_t i = 0; i < ret_count; i++) {
			if (i) at = put_name(at, ", ", 2);
			at = put_name(at, types[rets[i]].name, types[rets[i]].len);
		}
		*at++ = ')';
	}
	*at = '\0';
	*out_len = (uint16_t)(at - name);
	return name;
}

static int fn_same (const Type *t, const TypeId *params, uint16_t pc,
		const TypeId *rets, uint16_t rc, int variadic)
{
	if (t->param_count != pc || t->ret_count != rc || t->fn_variadic != variadic)
		return 0;
	for (uint16_t i = 0; i < pc; i++)
		if (t->params[i] != params[i]) return 0;
	for (uint16_t i = 0; i < rc; i++)
		if (t->rets[i] != rets[i]) return 0;
	return 1;
}

TypeId type_fn_of (const TypeId *params, uint16_t param_count,
		const TypeId *rets, uint16_t ret_count, int variadic)
{
	for (TypeId i = TYPE_COUNT; i < type_count; i++)
		if (types[i].kind == TK_FN
				&& fn_same(types + i, params, param_count, rets, ret_count, variadic))
			return i;

	uint16_t len;
	char *name = fn_name(params, param_count, rets, ret_count, variadic, &len);
	if (!name) return TYPE_ERROR;

	TypeId id = add_type(name, len, TK_FN, TYPE_VOID);
	if (id == TYPE_ERROR) return TYPE_ERROR;

	TypeId *pcopy = arena_alloc(&type_arena, sizeof(TypeId) * (param_count + 1u));
	TypeId *rcopy = arena_alloc(&type_arena, sizeof(TypeId) * (ret_count + 1u));
	for (uint16_t i = 0; i < param_count; i++) pcopy[i] = params[i];
	for (uint16_t i = 0; i < ret_count; i++) rcopy[i] = rets[i];

	types[id].params = pcopy;
	types[id].rets = rcopy;
	types[id].param_count = param_count;
	types[id].ret_count = ret_count;
	types[id].fn_variadic = variadic != 0;
	types[id].size = 0;
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

	uint16_t *slot = derived_slot(TK_ARRAY, base, count);
	if (*slot) return *slot;

	int dyn = type_dyn_rank(base);
	uint64_t total = dyn ? 8u * (1u + (uint64_t)dyn)
		: (uint64_t)types[base].size * count;
	if (total > UINT32_MAX) return TYPE_ERROR;

	uint16_t len;
	char *name = array_name(types + base, count, &len);
	if (!name) return TYPE_ERROR;

	TypeId id = add_type(name, len, TK_ARRAY, base);
	if (id == TYPE_ERROR) return TYPE_ERROR;
	types[id].size = (uint32_t)total;
	types[id].count = count;
	types[id].sign = 0;
	*slot = id;
	return id;
}

TypeId type_slice_of (TypeId base)
{
	if (base == TYPE_ERROR || base >= type_count || !types[base].size)
		return TYPE_ERROR;

	uint16_t *slot = derived_slot(TK_SLICE, base, 0);
	if (*slot) return *slot;

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
	types[id].size = 8u * (uint32_t)(2 + type_dyn_rank(base));
	types[id].count = 0;
	types[id].sign = 0;
	*slot = id;
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

int type_dyn_rank (TypeId id)
{
	int rank = 0;

	while (type_kind(id) == TK_ARRAY || type_kind(id) == TK_SLICE) {
		if (type_kind(id) == TK_SLICE) rank++;
		id = types[id].base;
	}
	return rank;
}

int type_is_mixed (TypeId id)
{
	int arrays = 0;
	int slices = 0;

	while (type_kind(id) == TK_ARRAY || type_kind(id) == TK_SLICE) {
		if (type_kind(id) == TK_SLICE) slices = 1;
		else arrays = 1;
		id = types[id].base;
	}
	return arrays && slices;
}

int type_is_aggregate (TypeId id)
{
	TypeKind kind = type_kind(id);

	return kind == TK_ARRAY || kind == TK_SLICE || kind == TK_STRUCT;
}

int type_scalar_leaves (TypeId id, int *budget)
{
	const Type *t = types + id;

	if (t->kind != TK_STRUCT) {
		(*budget)--;
		return !type_is_aggregate(id) && *budget >= 0;
	}
	if (t->is_union) return 0;
	for (uint16_t i = 0; i < t->field_count; i++) {
		if (!type_scalar_leaves(t->fields[i].type, budget)) return 0;
	}
	return 1;
}

uint32_t type_align (TypeId id)
{
	int has_slice = 0;
	TypeId cur = id;

	while (type_kind(cur) == TK_ARRAY || type_kind(cur) == TK_SLICE) {
		if (type_kind(cur) == TK_SLICE) has_slice = 1;
		cur = types[cur].base;
	}

	if (cur == TYPE_ERROR) return 8;
	if (types[cur].kind == TK_STRUCT) {
		uint32_t a = types[cur].align;
		if (has_slice && a < 8) a = 8;
		return a;
	}
	uint32_t size = types[cur].size;
	uint32_t a = size > 8 ? 8 : (size ? size : 1);
	if (has_slice && a < 8) a = 8;
	return a;
}

TypeId find_type (const char *name, uint16_t len)
{
	uint16_t *slot = name_slot(name, len);

	if (*slot) {
		const Type *t = types + *slot;

		if (t->len == len && memcmp(t->name, name, len) == 0) {
			types[*slot].used = 1;
			return *slot;
		}
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
		copy[i].decl = vars[i].decl;
		copy[i].len = vars[i].len;
		copy[i].used = vars[i].used;
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
		if (v->len != len || memcmp(v->name, name, len) != 0) continue;
		v->used = 1;
		types[id].used = 1;
		return v;
	}
	return NULL;
}

int bind_struct_type (TypeId id, const StructField *fields, uint16_t count, uint32_t size, uint32_t align)
{
	if (id < TYPE_COUNT || id >= type_count) return 0;

	StructField *copy = arena_alloc(&type_arena, sizeof(StructField) * (count + 1u));
	for (uint16_t i = 0; i < count; i++) {
		copy[i] = fields[i];
		copy[i].name = copy_name(fields[i].name, fields[i].len);
	}

	types[id].fields = copy;
	types[id].field_count = count;
	types[id].size = size;
	types[id].align = align;
	return 1;
}

StructField *find_field (TypeId id, const char *name, uint16_t len)
{
	if (type_kind(id) != TK_STRUCT) return NULL;

	for (uint16_t i = 0; i < types[id].field_count; i++) {
		StructField *f = types[id].fields + i;
		if (f->len != len || memcmp(f->name, name, len) != 0) continue;
		f->used = 1;
		types[id].used = 1;
		return f;
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

TypeId type_ptr_repr (TypeId id)
{
	TypeId repr = type_repr(id);

	return type_kind(repr) == TK_PTR ? repr : TYPE_ERROR;
}
