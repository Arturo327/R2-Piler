#ifndef SYMBOL_H
#define SYMBOL_H

#include "arena/arena.h"
#include <stdint.h>

#define NO_SYMBOL 0xFFFFFFFF

typedef enum {
	SYMBOL_FN,
	SYMBOL_VAR,
	SYMBOL_CONST,
	SYMBOL_PARAM
} SymKind;

typedef struct Symbol {
	char *name;
	
	uint32_t depth;
	uint32_t decl;

	uint16_t line;
	uint16_t col;
	uint16_t len;

	uint8_t kind;
	uint8_t type;
} Symbol;

typedef struct SymbolTable {
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

#endif
