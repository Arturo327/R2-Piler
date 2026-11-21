#ifndef MANGLE_H
#define MANGLE_H

#include <stddef.h>
#include <stdint.h>

#include "arena/arena.h"

typedef struct MangleName {
	const char *kind;
	const char *mod;
	const char *a;
	const char *b;
	uint32_t dup;
	uint16_t alen;
	uint16_t blen;
} MangleName;

int mangle_module_relative (const char *path, char *out, size_t cap);
int mangle_module (Arena *arena, const char *path, const char **out);
int mangle_module_name (Arena *arena, const char *name, uint16_t len, const char **out);
char *mangle_symbol (Arena *arena, const MangleName *m);
char *mangle_init (Arena *arena, const char *mod);
char *mangle_plain (Arena *arena, const char *name, uint16_t len);

#endif
