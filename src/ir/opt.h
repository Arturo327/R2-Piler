#ifndef OPT_H
#define OPT_H

#include "arena/arena.h"
#include "ir/cfg.h"
#include "ir/ir.h"

#define OPT_NO_BLOCK 0xFFFFFFFFu
#define OPT_CSE_MAX 32

typedef enum {
	NO_OPT = 0,
	OPT_REG,
	OPT_BASIC,
	OPT_FULL
} OptLevel;

typedef struct CseEntry {
	uint32_t dst;
	uint32_t src1;
	uint32_t src2;
	uint8_t op;
	TypeId type;
} CseEntry;

#define OPT_FWD_MAX 32

enum {
	FWD_PTR = 0,
	FWD_SLOT,
	FWD_GLOB
};

typedef struct FwdEntry {
	int64_t off;
	uint32_t base;
	uint32_t val;
	TypeId type;
	uint8_t kind;
} FwdEntry;

typedef struct OptFn {
	IRFn *fn;
	CfgBlock *blocks;

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
	uint32_t *gvis;
	uint32_t gvis_count;

	uint32_t *dstamp;
	uint32_t depoch;

	uint64_t *live;
	uint64_t *live_tmp;
	size_t live_cap;

	uint32_t *pred_cnt;
	uint32_t *pred_start;
	uint32_t *preds;
	uint32_t pred_cap;
	uint32_t block_cap;
	uint8_t *queued;

	CseEntry *cse;
	uint32_t cse_count;

	FwdEntry *fwd;
	uint32_t fwd_count;

	uint32_t block_count;
	uint32_t entry_end;
} OptFn;

typedef struct BufCaps {
	size_t lmap;
	size_t lrefs;
	size_t defs;
	size_t uses;
	size_t def_at;
	size_t blocks;
	size_t work;
	size_t pred_cnt;
	size_t pred_start;
	size_t preds;
	size_t queued;
	size_t kstamp;
	size_t kval;
	size_t gval;
	size_t gstamp;
	size_t dstamp;
	size_t live_tmp;
} BufCaps;

typedef struct Optimizer {
	Arena *arena;
	IR *ir;
	OptFn cur;
	Cfg cfg;

	uint32_t *lmap;
	uint32_t *lrefs;
	OptLevel level;
	BufCaps cap;
	uint32_t *gvis;
	uint32_t gvis_count;
} Optimizer;

void optimize_ir (IR *ir, Arena *a, OptLevel level);

#endif
