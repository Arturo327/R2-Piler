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
#include "mangle/mangle.h"
#include "r2i/r2i.h"
#include "header/header.h"

typedef struct CompilerOpts {
	char *path;
	char *out;
	const char *r2i_dir;
	const char *header_dir;
	const char **import_paths;
	int import_path_count;
	Arch arch;
	OptLevel opt_level;

	int r2i;
	int header;
	int werror;
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
	const char *mod;
} Compiler;

void compiler_init (Compiler *comp);
int compile (Compiler *c, CompilerOpts *opts);
void compiler_destroy (Compiler *comp);

#endif
