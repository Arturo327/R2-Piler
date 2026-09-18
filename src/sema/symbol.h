#ifndef SYMBOL_H
#define SYMBOL_H

#include "arena/arena.h"
#include <stdint.h>

#define NO_SYMBOL 0xFFFFFFFF

typedef enum {
	SYMBOL_FN = 0,
	SYMBOL_VAR,
	SYMBOL_PARAM
} SymKind;

typedef enum {
	INIT_PENDING = 0,
	INIT_CHECKING = 1,
	INIT_DONE = 2
} InitState;

typedef struct Symbol {
	char *name;
	
	uint32_t depth;
	uint32_t decl;
	uint32_t ir_id;
	uint32_t next;

	uint16_t line;
	uint16_t col;
	uint16_t len;

	uint8_t kind;
	uint8_t type;

	uint8_t assigned : 1;
	uint8_t state : 2;
} Symbol;

typedef struct SymbolTable {
	uint32_t *buckets;
	Symbol *symbols;
	uint32_t count;
	uint32_t cap;

	uint32_t *active;
	uint32_t act_count;
	uint32_t act_cap;
} SymbolTable;

void symtab_init (SymbolTable *t, Arena *arena);
uint32_t symtab_declare (SymbolTable *t, Arena *arena, Symbol sym);
uint32_t symtab_find (SymbolTable *t, char *name, uint16_t len);
void symtab_pop_scope (SymbolTable *t, uint32_t mark);

#endif
