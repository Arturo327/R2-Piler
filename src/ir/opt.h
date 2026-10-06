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
	OptBlock *blocks;

	uint32_t *lmap;
	uint32_t *defs;
	uint32_t *uses;

	uint32_t *pred_head;
	uint32_t *pred_next;
	uint32_t *pred_blk;
	uint32_t *pred_count;

	uint32_t block_count;
	uint32_t fn_idx;
	uint32_t reg_count;
	uint32_t label_cap;
} OptFn;

typedef struct Optimizer {
	Arena *arena;
	IR *ir;
	OptFn *fns;
	uint32_t fn_count;
	OptLevel level;
} Optimizer;

void optimize_ir (IR *ir, Arena *a, OptLevel level);

#endif
