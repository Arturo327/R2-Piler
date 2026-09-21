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

	uint32_t *init_order;
	uint32_t init_order_count;
	uint32_t init_order_cap;

	uint32_t depth;
	uint32_t init_node;
	uint32_t expr_depth;
	uint8_t curr_ret;
	uint8_t too_deep;
} Sema;

void sema_init (Sema *sema, Arena *arena, AST *ast, ErrorReporter *err);
void sema_run (Sema *sema);
void dump_symbols (SymbolTable *t);

#endif
