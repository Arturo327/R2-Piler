#include <string.h>

#include "sema/symbol.h"

#define SYMTAB_INIT_CAP 64

void symtab_init (SymbolTable *t, Arena *a)
{
	t->count = 0;
	t->cap = SYMTAB_INIT_CAP;
	t->symbols = arena_alloc(a, sizeof(Symbol) * t->cap);

	t->act_count = 0;
	t->act_cap = SYMTAB_INIT_CAP;
	t->active = arena_alloc(a, sizeof(uint32_t) * t->act_cap);
}

static void symtab_grow_storage (SymbolTable *t, Arena *arena)
{
	uint32_t old_cap = t->cap;
	t->cap <<= 1;
	t->symbols = arena_realloc(arena, t->symbols,
			old_cap * sizeof(Symbol), t->cap * sizeof(Symbol));
}

static void symtab_grow_active (SymbolTable *t, Arena *a)
{
	uint32_t old_cap = t->act_cap;
	t->act_cap <<= 1;
	t->active = arena_realloc(a, t->active,
			old_cap * sizeof(uint32_t), t->act_cap * sizeof(uint32_t));
}

uint32_t symtab_declare (SymbolTable *t, Arena *a, Symbol sym)
{
	if (t->count >= t->cap)
		symtab_grow_storage(t, a);
	if (t->act_count >= t->act_cap)
		symtab_grow_active(t, a);

	uint32_t idx = t->count++;
	t->symbols[idx] = sym;
	t->active[t->act_count++] = idx;
	return idx;
}

uint32_t symtab_find (SymbolTable *t, char *name, uint16_t len)
{
	uint32_t i = t->act_count;
	while (i > 0) {
		i--;
		uint32_t idx = t->active[i];
		Symbol *s = &t->symbols[idx];
		if (s->len == len && memcmp(s->name, name, len) == 0)
			return idx;
	}
	return NO_SYMBOL;
}
