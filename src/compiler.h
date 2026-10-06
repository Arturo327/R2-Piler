#ifndef COMPILER_H
#define COMPILER_H

#include "arena/arena.h"
#include "lexer/lexer.h"
#include "error/error.h"
#include "parser/parser.h"
#include "sema/sema.h"
#include "codegen/codegen.h"
#include "ir/ir.h"
#include "ir/opt.h"

typedef struct CompilerOpts {
	char *path;
	char *out;
	Arch arch;
	OptLevel opt_level;

	int dump_tokens;
	int dump_ast;
	int dump_symbols;
	int dump_ir;
	int dump_opt;
} CompilerOpts;

typedef struct Compiler {
	Arena arena;
	Arena ast_arena;
	Arena sym_arena;
	Arena ir_arena;
	Arena gen_arena;
	Arena opt_arena;

	ErrorReporter err;

	Lexer lexer;
	Parser parser;
	Sema sema;
	IR ir;
	CodeGen codegen;

	char *src;
} Compiler;

void compiler_init (Compiler *comp);
int compile (Compiler *c, CompilerOpts *opts);
void compiler_destroy (Compiler *comp);

#endif
