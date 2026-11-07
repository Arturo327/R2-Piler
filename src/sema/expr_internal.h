#ifndef EXPR_INTERNAL_H
#define EXPR_INTERNAL_H

#include "sema/common.h"

extern const uint8_t compound_base[NODE_COUNT];

static inline int is_integer_operand (TypeId type)
{
	TypeKind kind = type_kind(type);

	return kind == TK_INT || kind == TK_UNTYPED_INT;
}

static inline int is_pointer_operand (TypeId type)
{
	return type_kind(type) == TK_PTR || type == TYPE_NULL || type == TYPE_STR;
}

TypeId check_literal (Sema *s, uint32_t idx);
TypeId check_var_ref (Sema *s, uint32_t idx, int init_refs);
void mark_read (Sema *s, uint32_t idx);
void warn_null_deref (Sema *s, uint32_t idx, uint32_t child);
TypeId check_id (Sema *s, uint32_t idx);
TypeId check_assign (Sema *s, uint32_t idx);
TypeId check_compound_assign (Sema *s, uint32_t idx);

TypeId check_deref (Sema *s, uint32_t idx);
TypeId check_index (Sema *s, uint32_t idx);
TypeId check_addr (Sema *s, uint32_t idx);
TypeId check_sizeof (Sema *s, uint32_t idx);
TypeId check_dot (Sema *s, uint32_t idx);
TypeId check_fn_call (Sema *s, uint32_t idx);

TypeId check_unary (Sema *s, uint32_t idx);
TypeId check_cast (Sema *s, uint32_t idx);
TypeId check_ternary (Sema *s, uint32_t idx);
TypeId check_binary (Sema *s, uint32_t idx);

#endif
