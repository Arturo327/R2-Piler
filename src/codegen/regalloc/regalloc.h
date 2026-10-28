#ifndef REGALLOC_H
#define REGALLOC_H

#include <stdint.h>

#include "ir/ir.h"
#include "arena/arena.h"

#define VR_NO_HOME -1
#define VR_SPILL -2

enum {
	VR_NONE = 0,
	VR_CONST = 1,
	VR_MEM = 2,
	VR_ALIAS = 3
};

typedef enum {
	REG_COND_ANY = 0,
	REG_COND_FIXED,
	REG_COND_CLASSES,
	REG_COND_HINT
} RegCondType;

typedef struct OpCond {
	union {
		int16_t reg;
		uint64_t mask;
	};
	uint8_t type;
} OpCond;

typedef struct OpNeeds {
	OpCond src1;
	OpCond src2;
	OpCond dst;

	uint64_t destroy_mask;
} OpNeeds;

typedef struct Reg {
	int8_t *home;
	int64_t cval;

	uint32_t uses;
	uint32_t slot;
	uint8_t defs;

	uint8_t cstate;
	uint8_t spill;
} Reg;

typedef struct EdgeMove {
	uint32_t vreg;
	int8_t from;
	int8_t to;
} EdgeMove;

typedef struct EdgeMoves {
	uint32_t first;
	uint32_t fall;
	uint32_t taken;
} EdgeMoves;

enum {
	SC_DST = 0,
	SC_SRC1,
	SC_SRC2,
	SC_COND,
	SC_COUNT
};

typedef uint32_t (*ScratchFn) (void *user, IRInstr *in, const int8_t *homes);

typedef struct RegCtx {
	uint32_t *ins_block;
	uint64_t *save;
	uint64_t *free_regs;

	EdgeMoves *edges;
	EdgeMove *moves;
	OpNeeds *needs;

	uint32_t block_count;
	uint32_t move_count;
	uint32_t arg_reg_max;
	const uint8_t *arg_regs;

	void *user;
	uint32_t scratch_max;
	ScratchFn scratch_need;

	uint64_t callee_used;
	uint64_t callee_mask;
	uint64_t caller_mask;
} RegCtx;

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx,
		uint32_t reg_num, OpNeeds *needs, int spill_all);

#endif
