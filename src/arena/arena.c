#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "arena/arena.h"

#define ARENA_ALIGN 16
#define ARENA_DEFAULT_BLOCK (64 * 1024)

typedef struct ArenaBlock {
	struct ArenaBlock *next;
	size_t used;
	uint8_t data[];
} ArenaBlock;

static size_t align_up (size_t n)
{
	return (n + (ARENA_ALIGN - 1)) & ~(size_t)(ARENA_ALIGN - 1);
}

static ArenaBlock *new_block (size_t capacity)
{
	ArenaBlock *block = malloc(sizeof(ArenaBlock) + capacity);

	if (!block) {
		fprintf(stderr, "Not enough memory for arena block\n");
		exit(1);
	}
	block->next = NULL;
	block->used = 0;
	return block;
}

void arena_init (Arena *a)
{
	a->block_size = ARENA_DEFAULT_BLOCK;
	a->head = new_block(a->block_size);
	a->tail = a->head;
	a->cap = a->block_size;
	a->last_ptr = NULL;
	a->last_size = 0;
}

void *arena_alloc (Arena *a, size_t size)
{
	size_t aligned = align_up(size);

	if (aligned > a->cap - a->tail->used) {
		size_t cap = aligned > a->block_size ? aligned : a->block_size;
		a->tail->next = new_block(cap);
		a->tail = a->tail->next;
		a->cap = cap;
	}

	void *ptr = a->tail->data + a->tail->used;
	a->tail->used += aligned;
	a->last_ptr = ptr;
	a->last_size = aligned;
	return ptr;
}

void *arena_realloc (Arena *a, void *ptr, size_t old_size, size_t new_size)
{
	if (new_size <= old_size)
		return ptr;

	if (!ptr)
		return arena_alloc(a, new_size);

	if (ptr == a->last_ptr) {
		size_t grown = align_up(new_size);
		if (grown <= a->last_size)
			return ptr;

		size_t extra = grown - a->last_size;
		if (extra <= a->cap - a->tail->used) {
			a->tail->used += extra;
			a->last_size = grown;
			return ptr;
		}
	}

	void *new_ptr = arena_alloc(a, new_size);
	memcpy(new_ptr, ptr, old_size);
	return new_ptr;
}

void arena_destroy (Arena *a)
{
	ArenaBlock *block = a->head;
	while (block) {
		ArenaBlock *next = block->next;
		free(block);
		block = next;
	}
	memset(a, 0, sizeof(*a));
}
