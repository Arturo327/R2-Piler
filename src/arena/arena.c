#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "arena/arena.h"

#define ARENA_ALIGN 16
#define ARENA_DEFAULT_BLOCK (64 * 1024)

struct ArenaBlock {
	struct ArenaBlock *next;
	size_t used;
	uint8_t data[];
};

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
	a->marks = 0;
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

static void *regrow_block (Arena *a, size_t grown)
{
	ArenaBlock *old = a->tail;
	ArenaBlock *prev = a->head;
	ArenaBlock *blk;

	while (prev != old && prev->next != old)
		prev = prev->next;

	blk = realloc(old, sizeof(ArenaBlock) + grown);
	if (!blk) {
		fprintf(stderr, "Not enough memory for arena block\n");
		exit(1);
	}
	if (prev == old) a->head = blk;
	else prev->next = blk;

	blk->used = grown;
	a->tail = blk;
	a->cap = grown;
	a->last_ptr = blk->data;
	a->last_size = grown;
	return blk->data;
}

static void *grow_last (Arena *a, size_t new_size)
{
	size_t grown = align_up(new_size);
	size_t extra;

	if (grown <= a->last_size)
		return a->last_ptr;

	extra = grown - a->last_size;
	if (extra <= a->cap - a->tail->used) {
		a->tail->used += extra;
		a->last_size = grown;
		return a->last_ptr;
	}
	if (a->marks) return NULL;
	if (a->last_ptr == (void *)a->tail->data)
		return regrow_block(a, grown);
	return NULL;
}

void *arena_realloc (Arena *a, void *ptr, size_t old_size, size_t new_size)
{
	if (new_size <= old_size)
		return ptr;

	if (!ptr)
		return arena_alloc(a, new_size);

	if (ptr == a->last_ptr) {
		void *grown = grow_last(a, new_size);
		if (grown) return grown;
	}

	void *new_ptr = arena_alloc(a, new_size);
	memcpy(new_ptr, ptr, old_size);
	return new_ptr;
}

void *arena_alloc_block (Arena *a, size_t size)
{
	size_t aligned = align_up(size);
	ArenaBlock *blk = new_block(aligned);

	a->tail->next = blk;
	a->tail = blk;
	a->cap = aligned;
	blk->used = aligned;
	a->last_ptr = blk->data;
	a->last_size = aligned;
	return blk->data;
}

void *arena_realloc_block (Arena *a, void *ptr, size_t old_size, size_t new_size)
{
	ArenaBlock *prev = NULL;
	ArenaBlock *blk = a->head;
	ArenaBlock *grown;
	size_t aligned = align_up(new_size);

	if (new_size <= old_size) return ptr;
	while (blk && (void *)blk->data != ptr) {
		prev = blk;
		blk = blk->next;
	}
	if (!blk || a->marks) {
		void *out = arena_alloc(a, new_size);

		memcpy(out, ptr, old_size);
		return out;
	}
	grown = realloc(blk, sizeof(ArenaBlock) + aligned);
	if (!grown) {
		fprintf(stderr, "Not enough memory for arena block\n");
		exit(1);
	}
	if (prev) prev->next = grown;
	else a->head = grown;
	grown->used = aligned;
	if (a->tail == blk) {
		a->tail = grown;
		a->cap = aligned;
		a->last_ptr = grown->data;
		a->last_size = aligned;
	}
	return grown->data;
}

ArenaMark arena_mark (Arena *a)
{
	ArenaMark m;

	arena_alloc(a, 1);
	m.tail = a->tail;
	m.used = a->tail->used;
	m.cap = a->cap;
	a->marks++;
	return m;
}

void arena_release (Arena *a, const ArenaMark *m)
{
	ArenaBlock *block = m->tail->next;

	while (block) {
		ArenaBlock *next = block->next;

		free(block);
		block = next;
	}
	m->tail->next = NULL;
	m->tail->used = m->used;
	a->tail = m->tail;
	a->cap = m->cap;
	if (a->marks) a->marks--;
	a->last_ptr = NULL;
	a->last_size = 0;
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
