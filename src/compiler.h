#ifndef COMPILER_H
#define COMPILER_H

#include "arena/arena.h"
#include "lexer/lexer.h"
#include "error/error.h"
#include "parser/parser.h"

typedef struct CompilerOpts {
	const char *path;
	const char *out;
	int dump_tokens;
	int dump_ast;
} CompilerOpts;

typedef struct Compiler {
	Arena arena;
	Arena ast_arena;
	ErrorReporter err;
	char *src;
	Lexer lexer;
	Parser parser;
} Compiler;

void compiler_init (Compiler *comp);
int compile (Compiler *c, CompilerOpts *opts);
void compiler_destroy (Compiler *comp);

#endif
