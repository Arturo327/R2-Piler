#include <string.h>

#include "sema/symbol.h"

#define SYMTAB_INIT_CAP 64
#define SYMTAB_BUCKET_COUNT 1024
#define SYMTAB_BUCKET_MASK (SYMTAB_BUCKET_COUNT - 1)

static uint32_t hash_name (char *name, uint16_t len)
{
	uint32_t hash = 2166136261u;

	for (uint16_t i = 0; i < len; i++) {
		hash ^= (uint8_t)name[i];
		hash *= 16777619u;
	}

	return hash;
}

void symtab_init (SymbolTable *t, Arena *a)
{
	t->count = 0;
	t->cap = SYMTAB_INIT_CAP;
	t->symbols = arena_alloc(a, sizeof(Symbol) * t->cap);

	t->act_count = 0;
	t->act_cap = SYMTAB_INIT_CAP;
	t->active = arena_alloc(a, sizeof(uint32_t) * t->act_cap);

	t->buckets = arena_alloc(a, sizeof(uint32_t) * SYMTAB_BUCKET_COUNT);
	for (uint32_t i = 0; i < SYMTAB_BUCKET_COUNT; i++)
		t->buckets[i] = NO_SYMBOL;
	t->bucket_count = SYMTAB_BUCKET_COUNT;
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

static void symtab_rehash (SymbolTable *t, Arena *a)
{
	uint32_t count = t->bucket_count << 1;
	uint32_t *b = arena_alloc(a, sizeof(uint32_t) * count);

	for (uint32_t i = 0; i < count; i++)
		b[i] = NO_SYMBOL;

	for (uint32_t i = 0; i < t->act_count; i++) {
		Symbol *s = &t->symbols[t->active[i]];
		uint32_t h = hash_name(s->name, s->len) & (count - 1);

		s->next = b[h];
		b[h] = t->active[i];
	}
	t->buckets = b;
	t->bucket_count = count;
}

uint32_t symtab_declare (SymbolTable *t, Arena *a, Symbol sym)
{
	if (t->act_count >= t->bucket_count * 2)
		symtab_rehash(t, a);

	if (t->count >= t->cap)
		symtab_grow_storage(t, a);
	if (t->act_count >= t->act_cap)
		symtab_grow_active(t, a);

	uint32_t idx = t->count++;
	uint32_t bucket = hash_name(sym.name, sym.len) & (t->bucket_count - 1);

	sym.next = t->buckets[bucket];
	t->symbols[idx] = sym;
	t->buckets[bucket] = idx;

	t->active[t->act_count++] = idx;
	return idx;
}

uint32_t symtab_find (SymbolTable *t, char *name, uint16_t len)
{
	uint32_t bucket = hash_name(name, len) & (t->bucket_count - 1);
	uint32_t idx = t->buckets[bucket];

	while (idx != NO_SYMBOL) {
		Symbol *s = &t->symbols[idx];
		if (s->len == len && memcmp(s->name, name, len) == 0)
			return idx;
		idx = s->next;
	}

	return NO_SYMBOL;
}

void symtab_pop_scope (SymbolTable *t, uint32_t mark)
{
	uint32_t i = t->act_count;

	while (i > mark) {
		i--;
		uint32_t idx = t->active[i];
		Symbol *s = &t->symbols[idx];
		uint32_t bucket = hash_name(s->name, s->len) & (t->bucket_count - 1);

		t->buckets[bucket] = s->next;
	}

	t->act_count = mark;
}
