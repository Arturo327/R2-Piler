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

uint32_t parse_expr (Parser *p, int min_prec);
uint32_t parse_cast (Parser *p);
uint32_t maybe_inc_dec (Parser *p, uint32_t node);
uint32_t parse_postfix (Parser *p, uint32_t node);

TypeId parse_type (Parser *p, int allow_void, uint32_t owner, uint16_t *type_node);
uint32_t parse_sizeof (Parser *p);

uint32_t parse_statement (Parser *p);
uint32_t parse_block (Parser *p);

uint32_t parse_fn_decl (Parser *p);
uint32_t parse_extern_decl (Parser *p);
uint32_t parse_enum (Parser *p);
uint32_t parse_type_dec (Parser *p, int is_newtype);
uint32_t parse_struct (Parser *p);

#endif
