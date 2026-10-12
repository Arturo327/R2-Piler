#ifndef REGALLOC_H
#define REGALLOC_H

#include <stdint.h>

#include "arena/arena.h"
#include "ir/ir.h"

#define RA_NO_HOME -1
#define RA_SPILL -2

enum {
	RA_NONE = 0,
	RA_CONST = 1,
	RA_MEM = 2,
	RA_ALIAS = 3
};

typedef struct Reg {
	int8_t *home;
	int64_t cval;

	uint32_t uses;
	uint32_t slot;
	uint8_t defs;

	uint8_t cstate;
	uint8_t spill;
} Reg;

typedef struct RegCtx {
	uint32_t *ins_block;
	uint16_t *save;
	uint32_t block_count;
	uint16_t callee_used;
} RegCtx;

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs,
		RegCtx *ctx, uint32_t reg_num, int spill_all);

#endif
