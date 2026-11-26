#ifndef PARSER_COMMON_H
#define PARSER_COMMON_H

#include "parser/parser.h"

void advance (Parser *p);
void syncro (Parser *p, Token start);
int enter_depth (Parser *p, const char *what);
ErrorLoc token_loc (Token t);
int consume (Parser *p, TokenType type, const char *msg);
uint32_t new_node (Parser *p, NodeType type, uint16_t line, uint16_t col);
void append_child (Parser *p, uint32_t parent, uint32_t *last, uint32_t child);
char *qualify_name (Parser *p, Token first, uint16_t *len, int *ok);

uint32_t parse_expr (Parser *p, int min_prec);
uint32_t parse_cast (Parser *p);
uint32_t maybe_inc_dec (Parser *p, uint32_t node);
uint32_t parse_postfix (Parser *p, uint32_t node);

TypeId parse_type (Parser *p, int allow_void, uint32_t owner, uint16_t *type_node);
TypeId parse_new_type (Parser *p, uint32_t owner, uint16_t *type_node);
uint32_t parse_sizeof (Parser *p);
uint32_t parse_alignof (Parser *p);
uint32_t parse_offsetof (Parser *p);
uint32_t parse_reset (Parser *p);

uint32_t parse_statement_inner (Parser *p, int root);
uint32_t parse_import (Parser *p, int root);
uint32_t parse_from (Parser *p, int root);
uint32_t parse_pub (Parser *p, int root);
uint32_t parse_export (Parser *p, int root);
void mark_decl_flags (Parser *p, uint32_t node, uint16_t flags);
uint32_t parse_module (Parser *p);
uint32_t parse_statement (Parser *p);
uint32_t parse_block (Parser *p);
uint32_t parse_brace_init (Parser *p);
uint32_t parse_decl (Parser *p, int is_arr);
uint32_t parse_static_decl (Parser *p);
uint32_t parse_for (Parser *p);
uint32_t parse_body (Parser *p);
void parse_loop_else (Parser *p, uint32_t node);

uint32_t parse_fn_decl (Parser *p);
uint32_t parse_extern_decl (Parser *p);
uint32_t parse_enum (Parser *p);
uint32_t parse_type_dec (Parser *p, int is_newtype);
uint32_t parse_struct (Parser *p, int packed);
uint32_t parse_class (Parser *p);
uint32_t parse_priv_method (Parser *p);

#endif
