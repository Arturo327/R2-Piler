#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct ArenaMark {
	ArenaBlock *tail;
	size_t used;
	size_t cap;
} ArenaMark;

typedef struct Arena {
	ArenaBlock *head;
	ArenaBlock *tail;
	size_t cap;
	size_t block_size;
	void *last_ptr;
	size_t last_size;
	uint32_t marks;
} Arena;

void arena_init (Arena *a);
void *arena_alloc (Arena *a, size_t size);
void *arena_realloc (Arena *a, void *ptr, size_t old_size, size_t new_size);
ArenaMark arena_mark (Arena *a);
void arena_release (Arena *a, const ArenaMark *m);
void arena_destroy (Arena *a);

#endif
