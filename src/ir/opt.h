#ifndef OPT_H
#define OPT_H

#include "arena/arena.h"
#include "ir/ir.h"

#define OPT_NO_BLOCK 0xFFFFFFFFu
#define OPT_CSE_MAX 32

typedef enum {
	NO_OPT = 0,
	OPT_BASIC,
	OPT_FULL
} OptLevel;

typedef struct OptBlock {
	uint64_t *live_in;
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

	uint32_t *kstamp;
	int64_t *kval;
	uint32_t epoch;

	uint32_t *gval;
	uint32_t *gstamp;
	uint32_t gepoch;

	uint32_t *dstamp;
	uint32_t depoch;

	uint64_t *live;
	uint64_t *live_tmp;
	size_t live_cap;

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
