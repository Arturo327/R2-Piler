#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

#include "arena/arena.h"
#include "parser/ast.h"
#include "error/error.h"

typedef struct Parser {
	uint8_t had_error;
	uint8_t panic_mode;

	Token curr;
	Token prev;
	AST ast;
	
	Lexer *lexer;
	ErrorReporter *err;
	Arena *arena;
} Parser;

void init_parser (Parser *parser, Lexer *lexer, Arena *arena, ErrorReporter *err);
void parse (Parser *parser);

#endif
