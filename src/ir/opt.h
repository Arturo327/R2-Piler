#ifndef OPT_H
#define OPT_H

#include "arena/arena.h"
#include "ir/ir.h"

#define OPT_NO_BLOCK 0xFFFFFFFFu

typedef enum {
	NO_OPT = 0,
	OPT_BASIC,
	OPT_FULL
} OptLevel;

typedef struct OptBlock {
	uint32_t start;
	uint32_t count;
	uint32_t succ[2];
	uint32_t succ_count;
	uint8_t reachable;
} OptBlock;

typedef struct OptFn {
	IRFn *fn;
	OptBlock *blocks;

	uint32_t *lmap;
	uint32_t *lrefs;
	uint32_t *defs;
	uint32_t *uses;
	uint32_t *def_at;
	uint32_t *work;

	uint32_t block_count;
} OptFn;

typedef struct Optimizer {
	Arena *arena;
	IR *ir;
	OptFn cur;
	uint32_t *lmap;
	uint32_t *lrefs;
	OptLevel level;
} Optimizer;

void optimize_ir (IR *ir, Arena *a, OptLevel level);

#endif
