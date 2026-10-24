#ifndef RA_COMMON_H
#define RA_COMMON_H

#include <stdint.h>

#include "codegen/regalloc/regalloc.h"

typedef struct ColorRegs {
	Arena *arena;
	IR *ir;
	IRFn *fn;
	Reg *regs;
	RegCtx *ctx;
	Cfg *cfg;
	uint32_t num_regs;
} ColorRegs;

typedef struct CRNode {
	uint64_t avail;
	uint32_t grad;
	uint32_t cost;

	int8_t home;
	int8_t pref;
	uint8_t colored;
} CRNode;

typedef struct Graph {
	CRNode *nodes;
	ColorRegs *cr;
	
	uint32_t *offsets;
	uint32_t *nbrs;
	uint32_t node_num;
	uint32_t nbrs_cap;

	uint64_t *live;
	uint32_t live_num;

	uint64_t pal;
	uint32_t blk;
} Graph;

Graph *make_graph (ColorRegs *cr);
void color_graph (Graph *graph);

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg, uint32_t reg_num);

#endif
