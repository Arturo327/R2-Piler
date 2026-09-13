#ifndef SEMA_H
#define SEMA_H

#include <stdint.h>

#include "arena/arena.h"
#include "error/error.h"
#include "parser/ast.h"
#include "sema/symbol.h"

typedef struct Sema {
	SymbolTable table;

	AST *ast;
	ErrorReporter *err;
	Arena *arena;

	uint32_t depth;
	uint8_t curr_ret;
} Sema;

void sema_init (Sema *sema, Arena *arena, AST *ast, ErrorReporter *err);
void sema_run (Sema *sema);
void dump_symbols (SymbolTable *t);

#endif
