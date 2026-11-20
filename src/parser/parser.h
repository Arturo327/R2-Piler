#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

#include "arena/arena.h"
#include "parser/ast.h"
#include "error/error.h"
#include "lexer/lexer.h"

typedef struct Parser {
	Token curr;
	Token prev;
	AST ast;
	
	Lexer *lexer;
	ErrorReporter *err;
	Arena *arena;

	uint16_t depth;
	uint8_t panic_mode;
	uint8_t at_root;
	uint8_t r2i_mode;
} Parser;

void init_parser (Parser *parser, Lexer *lexer, Arena *arena, ErrorReporter *err);
void init_parser_shared (Parser *parser, Lexer *lexer, Arena *arena, ErrorReporter *err, AST *ast);
void dump_ast (AST *ast);
void parse (Parser *parser);

#endif
