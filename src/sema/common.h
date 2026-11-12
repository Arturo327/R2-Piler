#ifndef COMMON_H
#define COMMON_H

#include "sema/sema.h"

ErrorLoc node_loc (ASTNode *n);
TypeId resolve_type_node (Sema *s, ASTNode *n);
TypeId resolve_type_spec (Sema *s, ASTNode *owner, uint8_t *done, int *deferred);
void resolve_fn_types (Sema *s, uint32_t idx);
uint32_t sema_ret_count (Sema *s, uint32_t ret);
uint32_t sema_ret_at (Sema *s, uint32_t ret, uint32_t i);
int enum_const_value (Sema *s, uint32_t idx, TypeId enum_id, uint64_t *out);
uint32_t sema_wrap_deref (Sema *s, uint32_t child, TypeId to_type);

uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		TypeId data_type, uint32_t decl_node);
void sema_decl_type (Sema *s, uint32_t idx);
void sema_resolve_types (Sema *s);
void sema_finalize_types (Sema *s);
uint32_t sema_wrap_cast (Sema *s, uint32_t child, TypeId to_type);
void check_global_var_init (Sema *s, uint32_t idx);

void record_init_order (Sema *s, uint32_t idx);
void record_static (Sema *s, uint32_t idx);
void check_var_init_type (Sema *s, ASTNode *n, TypeId init_type);
void check_storage_decl (Sema *s, ASTNode *n);
void check_array_init (Sema *s, uint32_t decl_idx);
void check_array_init_at (Sema *s, TypeId arr, uint32_t init_idx);
void sema_decl_struct (Sema *s, uint32_t idx);
void sema_layout_structs (Sema *s);
void check_struct_init (Sema *s, uint32_t init_idx, TypeId st);
int as_struct_init (Sema *s, uint32_t idx, TypeId st);

const char *type_display_name (TypeId type);
void warn_bitfield_truncation (Sema *s, const StructField *f, uint32_t value);
uint32_t force_cast (Sema *s, uint32_t child, TypeId child_type, TypeId target, int *ok);
TypeId finalize_type (Sema *s, uint32_t idx, TypeId type);
TypeId try_fold_unary (Sema *s, uint32_t idx);
void try_fold_binary (Sema *s, uint32_t idx);
int is_literal_node (Sema *s, uint32_t idx);
int lit_node_bits (Sema *s, ASTNode *n, uint64_t *out);
int unify_operands (Sema *s, ASTNode *n, uint32_t *left, uint32_t *right, TypeId *l, TypeId *r);

TypeId check_expr (Sema *s, uint32_t idx);
int check_cond (Sema *s, uint32_t cond, int alw_err);
int expr_has_effect (Sema *s, uint32_t idx);
int is_null_value (Sema *s, uint32_t idx);
void check_inc_dec (Sema *s, uint32_t idx);
void sym_write (Sema *s, uint32_t sym, uint32_t node);
void check_multi_assign (Sema *s, uint32_t idx);
void sym_set_stack_slice (Sema *s, uint32_t sym, uint32_t rhs);
void warn_stack_return (Sema *s, uint32_t val, TypeId ret);
int sema_array_slice_compat (TypeId arg, TypeId param);
int sema_implicit_cast_ok (TypeId from, TypeId to);
void check_fn_dec (Sema *s, uint32_t idx);
void check_extern_fn (Sema *s, uint32_t idx);

#endif
