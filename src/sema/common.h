#ifndef COMMON_H
#define COMMON_H

#include "sema/sema.h"

ErrorLoc node_loc (ASTNode *n);
TypeId resolve_type_node (Sema *s, ASTNode *n);
void resolve_fn_types (Sema *s, uint32_t idx);
int enum_const_value (Sema *s, uint32_t idx, TypeId enum_id, uint64_t *out);
uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		TypeId data_type, uint32_t decl_node);
void sema_decl_type (Sema *s, uint32_t idx);
void sema_resolve_types (Sema *s);
void sema_finalize_types (Sema *s);
uint32_t sema_wrap_cast (Sema *s, uint32_t child, TypeId to_type);
void check_global_var_init (Sema *s, uint32_t idx);
void check_var_init_type (Sema *s, ASTNode *n, TypeId init_type);

uint32_t force_cast (Sema *s, uint32_t child, TypeId child_type, TypeId target, int *ok);
TypeId finalize_type (Sema *s, uint32_t idx, TypeId type);
TypeId try_fold_unary (Sema *s, uint32_t idx);
void try_fold_binary (Sema *s, uint32_t idx);
int is_literal_node (Sema *s, uint32_t idx);
int lit_node_bits (Sema *s, ASTNode *n, uint64_t *out);
int unify_operands (Sema *s, ASTNode *n, uint32_t *left, uint32_t *right, TypeId *l, TypeId *r);

TypeId check_expr (Sema *s, uint32_t idx);
void check_inc_dec (Sema *s, uint32_t idx);
void check_fn_dec (Sema *s, uint32_t idx);

#endif
