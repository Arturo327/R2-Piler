#ifndef HEADER_INTERNAL_H
#define HEADER_INTERNAL_H

#include "header/header.h"

void h_printf (CHeader *h, const char *fmt, ...);
void h_indent (CHeader *h, int depth);

void h_collect_all (CHeader *h);
void h_make_types (CHeader *h);
void h_make_decls (CHeader *h);
void h_print_type (CHeader *h, TypeId type);
void h_print_pointee (CHeader *h, TypeId type);
void h_print_named_type (CHeader *h, TypeId type, const char *name, int len);
void h_make_param (CHeader *h, ASTNode *n, int names);
void h_make_params (CHeader *h, uint32_t args);

#endif
