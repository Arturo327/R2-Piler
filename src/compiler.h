#ifndef COMPILER_H
#define COMPILER_H

#include "arena/arena.h"
#include "lexer/lexer.h"
#include "error/error.h"
#include "parser/parser.h"
#include "sema/sema.h"
#include "ir/ir.h"

typedef struct CompilerOpts {
	char *path;
	char *out;
	int dump_tokens;
	int dump_ast;
	int dump_symbols;
	int dump_ir;
} CompilerOpts;

typedef struct Compiler {
	Arena arena;
	Arena ast_arena;
	Arena sym_arena;
	Arena ir_arena;
	ErrorReporter err;

	Lexer lexer;
	Parser parser;
	Sema sema;
	IR ir;

	char *src;
} Compiler;

void compiler_init (Compiler *comp);
int compile (Compiler *c, CompilerOpts *opts);
void compiler_destroy (Compiler *comp);

#endif
