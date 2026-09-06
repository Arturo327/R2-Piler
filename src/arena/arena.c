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

typedef struct Arena {
	ArenaBlock *head;
	ArenaBlock *tail;
	size_t cap;
	size_t block_size;
	void *last_ptr;
	size_t last_size;
} Arena;

static Arena arena = {0};

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

void arena_init (void)
{
	arena.block_size = ARENA_DEFAULT_BLOCK;
	arena.head = new_block(arena.block_size);
	arena.tail = arena.head;
	arena.cap = arena.block_size;
}

void *arena_alloc (size_t size)
{
	size_t aligned = align_up(size);

	if (aligned > arena.cap - arena.tail->used) {
		size_t cap = aligned > arena.block_size ? aligned : arena.block_size;
		arena.tail->next = new_block(cap);
		arena.tail = arena.tail->next;
		arena.cap = cap;
	}

	void *ptr = arena.tail->data + arena.tail->used;
	arena.tail->used += aligned;
	return ptr;
}

void *arena_realloc (void *ptr, size_t old_size, size_t new_size)
{
	if (new_size <= old_size)
		return ptr;

	if (ptr == NULL)
		return arena_alloc(new_size);

	if (ptr == arena.last_ptr) {
		size_t grown = align_up(new_size);
		if (grown <= arena.last_size)
			return ptr;

		size_t extra = grown - arena.last_size;
		if (extra <= arena.cap - arena.tail->used) {
			arena.tail->used += extra;
			arena.last_size = grown;
			return ptr;
		}
	}

	void *new_ptr = arena_alloc(new_size);
	memcpy(new_ptr, ptr, old_size);
	return new_ptr;
}

void arena_destroy (void)
{
	ArenaBlock *block = arena.head;
	while (block) {
		ArenaBlock *next = block->next;
		free(block);
		block = next;
	}
	arena.head = NULL;
	arena.tail = NULL;
	arena.cap = 0;
	arena.block_size = 0;
}
