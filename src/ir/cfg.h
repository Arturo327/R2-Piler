#ifndef IR_CFG_H
#define IR_CFG_H

#include <stdint.h>

#include "arena/arena.h"
#include "ir/ir.h"

#define CFG_NO_BLOCK 0xFFFFFFFFu

typedef struct CfgBlock {
	uint64_t *live_in;
	uint32_t start;
	uint32_t count;
	uint32_t succ[2];
	uint32_t succ_count;
	uint8_t reachable;
} CfgBlock;

typedef struct Cfg {
	CfgBlock *blocks;
	uint32_t *lmap;
	uint32_t *lrefs;
	uint32_t *work;
	uint32_t block_count;
	uint8_t live_full;
} Cfg;

void cfg_fill_blocks (IR *ir, IRFn *fn, CfgBlock *blocks, uint32_t *block_count);
uint32_t cfg_entry_len (IR *ir, IRFn *fn);
void cfg_build_lmap (IR *ir, Cfg *cfg);
uint32_t cfg_label_block (IR *ir, Cfg *cfg, uint32_t label);
void cfg_build_edges (IR *ir, Cfg *cfg);
void cfg_mark_reachable (Cfg *cfg);

int cfg_live_step (IRInstr *in, uint64_t *live);
void cfg_live_out (Cfg *cfg, uint32_t b, uint64_t *out, uint32_t words);
void cfg_live_solve (IR *ir, Cfg *cfg, uint64_t *live, uint64_t *tmp, uint32_t words);
void cfg_live_solve_full (IR *ir, Cfg *cfg, uint64_t *live, uint64_t *tmp, uint32_t words);

typedef struct OptFn OptFn;
void opt_cfg (OptFn *f, Cfg *cfg);

#endif
