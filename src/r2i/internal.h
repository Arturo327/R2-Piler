#ifndef R2I_INTERNAL_H
#define R2I_INTERNAL_H

#include "r2i/r2i.h"

void r2i_printf (R2i *r, const char *fmt, ...);
void r2i_indent (R2i *r, int depth);

void r2i_build_types (R2i *r);
void r2i_collect_imports (R2i *r);
void r2i_print_type (R2i *r, TypeId type);

void r2i_make_decls (R2i *r);
void r2i_make_init (R2i *r);
void r2i_make_struct (R2i *r, ASTNode *n);
void r2i_make_enum (R2i *r, ASTNode *n);
void r2i_make_alias (R2i *r, ASTNode *n, int is_newtype);
void r2i_make_global (R2i *r, ASTNode *n, int is_extern);
void r2i_make_fn (R2i *r, ASTNode *n, int is_extern);

void r2i_print_expr (R2i *r, uint32_t idx);

#endif
