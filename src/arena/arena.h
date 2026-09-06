#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct Arena {
	ArenaBlock *head;
	ArenaBlock *tail;
	size_t cap;
	size_t block_size;
	void *last_ptr;
	size_t last_size;
} Arena;

void arena_init (Arena *a);
void *arena_alloc (Arena *a, size_t size);
void *arena_realloc (Arena *a, void *ptr, size_t old_size, size_t new_size);
void arena_destroy (Arena *a);

#endif
