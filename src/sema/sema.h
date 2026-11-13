#ifndef SEMA_H
#define SEMA_H

#include <stdint.h>

#include "arena/arena.h"
#include "error/error.h"
#include "parser/ast.h"
#include "sema/symbol.h"

typedef struct DeadWrite {
	uint32_t sym;
	uint32_t node;
} DeadWrite;

typedef struct AggPlace {
	uint32_t sym;
	uint32_t place;
	uint32_t node;
	uint32_t slot;
} AggPlace;

typedef struct Sema {
	SymbolTable table;

	AST *ast;
	ErrorReporter *err;
	Arena *arena;

	uint32_t *init_order;
	uint32_t init_order_count;
	uint32_t init_order_cap;

	uint32_t *statics;
	uint32_t static_count;
	uint32_t static_cap;

	DeadWrite *dead;
	uint32_t dead_count;
	uint32_t dead_cap;

	AggPlace *places;
	uint32_t place_count;
	uint32_t place_cap;

	AggPlace *dead_places;
	uint32_t dead_place_count;
	uint32_t dead_place_cap;

	uint32_t write_root;
	uint32_t addr_operand;
	uint32_t curr_fn;

	uint32_t depth;
	uint32_t init_node;
	uint32_t expr_depth;
	TypeId curr_ret;
	uint32_t curr_ret_list;
	uint8_t too_deep;
} Sema;

void sema_init (Sema *sema, Arena *arena, AST *ast, ErrorReporter *err);
void sema_run (Sema *sema);
void dump_symbols (Sema *s);

#endif
