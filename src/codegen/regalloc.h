#ifndef REGALLOC_H
#define REGALLOC_H

#include <stdint.h>

#include "arena/arena.h"
#include "ir/ir.h"

#define VR_NO_HOME -1
#define VR_SPILL -2

enum {
	VR_NONE = 0,
	VR_CONST = 1,
	VR_MEM = 2,
	VR_ALIAS = 3
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
	uint16_t *free_regs;

	uint32_t block_count;
	uint16_t callee_used;
	uint16_t callee_mask;
} RegCtx;

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs,
		RegCtx *ctx, uint32_t reg_num, int spill_all);

#endif
