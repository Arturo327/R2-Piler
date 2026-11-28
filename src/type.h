#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>

#define TYPE_MAX 16384
#define TYPE_CHAR TYPE_i8

typedef uint16_t TypeId;

_Static_assert(TYPE_MAX <= UINT16_MAX, "TypeId is uint16_t");

typedef enum {
	TK_INVALID = 0,
	TK_INT,
	TK_ENUM,
	TK_ALIAS,
	TK_NEWTYPE,
	TK_UNTYPED_INT,
	TK_VOID,
	TK_ERROR,
	TK_STRUCT,
	TK_CLASS,
	TK_PTR,
	TK_FN,
	TK_ARRAY,
	TK_SLICE,
	TK_NULL,
	TK_STR
} TypeKind;

typedef enum {
	TYPE_VOID = 0,
	TYPE_i8,
	TYPE_u8,
	TYPE_i16,
	TYPE_u16,
	TYPE_i32,
	TYPE_u32,
	TYPE_i64,
	TYPE_u64,
	TYPE_UNTYPED_INT,
	TYPE_UNTYPED_UINT,
	TYPE_UNTYPED_CHAR,
	TYPE_ERROR,
	TYPE_NULL,
	TYPE_STR,
	TYPE_ANY,
	TYPE_COUNT
} DataType;

typedef struct EnumVariant {
	const char *name;
	uint64_t value;
	uint32_t decl;
	uint16_t len;
	uint8_t used;
} EnumVariant;

typedef struct StructField {
	const char *name;
	uint32_t offset;
	uint32_t decl;
	TypeId type;
	uint16_t len;
	uint16_t grp;
	uint16_t alt;
	uint8_t bits;
	uint8_t bit_off;
	uint8_t used;
} StructField;

typedef struct TypeDesc {
	const char *name;
	uint16_t len;
	uint32_t size;
	uint32_t count;
	uint8_t sign;
	uint8_t kind;
	uint8_t builtin;
	uint8_t used;
	uint8_t exposed;
	uint8_t is_const;

	TypeId base;
	TypeId plain;
	uint16_t variant_count;
	EnumVariant *variants;

	StructField *fields;
	uint32_t align;
	uint32_t decl;
	uint16_t field_count;
	uint8_t layout;
	uint8_t broken;
	uint8_t is_union;
	uint8_t is_packed;

	TypeId *params;
	TypeId *rets;
	uint16_t param_count;
	uint16_t ret_count;
	uint8_t fn_variadic;
} TypeDesc;

typedef TypeDesc Type;

extern Type types[TYPE_MAX];
extern uint16_t type_count;

void init_types (void);
void types_destroy (void);

TypeId add_type (const char *name, uint16_t len, TypeKind kind, TypeId base);
TypeId type_ptr_to (TypeId base);
TypeId type_fn_of (const TypeId *params, uint16_t param_count,
		const TypeId *rets, uint16_t ret_count, int variadic);
TypeId type_array_of (TypeId base, uint32_t count);
TypeId type_slice_of (TypeId base);
TypeId type_flat_element (TypeId id);

TypeId type_const_of (TypeId base);
TypeId type_unconst_of (TypeId id);
int type_is_const (TypeId id);
void types_refresh_const (void);

int type_array_rank (TypeId id);
int type_slice_rank (TypeId id);
int type_dyn_rank (TypeId id);
int type_is_mixed (TypeId id);
int type_is_aggregate (TypeId id);
int type_scalar_leaves (TypeId id, int *budget);

uint32_t type_align (TypeId id);
int bind_struct_type (TypeId id, const StructField *fields, uint16_t count, uint32_t size, uint32_t align);
StructField *find_field (TypeId id, const char *name, uint16_t len);

TypeId find_type (const char *name, uint16_t len);
int bind_enum_type (TypeId id, const EnumVariant *vars, uint16_t count);
EnumVariant *find_variant (TypeId id, const char *name, uint16_t len);

TypeKind type_kind (TypeId id);
int type_is_nominal (TypeId id);
int type_is_struct_like (TypeId id);
TypeId type_canon (TypeId id);
TypeId type_repr (TypeId id);
TypeId type_ptr_repr (TypeId id);

#endif
