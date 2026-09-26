#ifndef COMMON_H
#define COMMON_H

#include "sema/sema.h"

ErrorLoc node_loc (ASTNode *n);
uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		uint8_t data_type, uint32_t decl_node);
uint32_t sema_wrap_cast (Sema *s, uint32_t child, uint8_t to_type);
void check_global_var_init (Sema *s, uint32_t idx);
void check_var_init_type (Sema *s, ASTNode *n, uint8_t init_type);

uint32_t force_cast (Sema *s, uint32_t child, uint8_t child_type, uint8_t target, int *ok);
uint8_t finalize_type (Sema *s, uint32_t idx, uint8_t type);
uint8_t try_fold_unary (Sema *s, uint32_t idx);
void try_fold_binary (Sema *s, uint32_t idx);
int is_literal_node (Sema *s, uint32_t idx);
int unify_operands (Sema *s, ASTNode *n, uint32_t *left, uint32_t *right,
		uint8_t *l, uint8_t *r);

uint8_t check_expr (Sema *s, uint32_t idx);
void check_fn_dec (Sema *s, uint32_t idx);

#endif
