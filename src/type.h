#ifndef TYPE_H
#define TYPE_H

#include <stdint.h>

#define TYPE_MAX 4096
#define TYPE_CHAR TYPE_i8

typedef uint16_t TypeId;

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
	TK_CLASS
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
	TYPE_COUNT
} DataType;

typedef struct EnumVariant {
	const char *name;
	uint64_t value;
	uint16_t len;
} EnumVariant;

typedef struct TypeDesc {
	const char *name;
	uint16_t len;
	uint8_t size;
	uint8_t sign;
	uint8_t kind;
	uint8_t builtin;
	TypeId base;
	uint16_t variant_count;
	EnumVariant *variants;
} TypeDesc;

typedef TypeDesc Type;

extern Type types[TYPE_MAX];
extern uint16_t type_count;

void init_types (void);
void types_destroy (void);

TypeId add_type (const char *name, uint16_t len, TypeKind kind, TypeId base);
TypeId find_type (const char *name, uint16_t len);
int bind_enum_type (TypeId id, const EnumVariant *vars, uint16_t count);
EnumVariant *find_variant (TypeId id, const char *name, uint16_t len);

TypeKind type_kind (TypeId id);
int type_is_nominal (TypeId id);
TypeId type_canon (TypeId id);
TypeId type_repr (TypeId id);

#endif
