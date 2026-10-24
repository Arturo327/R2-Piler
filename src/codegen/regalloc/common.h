#ifndef RA_COMMON_H
#define RA_COMMON_H

#include <stdint.h>

#include "codegen/regalloc/regalloc.h"

#define GRAPH_MAX_NODES 1024u
#define GRAPH_NO_NODE 0xFFFFFFFFu
#define GRAPH_SCRATCH 2u

enum {
	CRN_ACTIVE = 0,
	CRN_STACKED
};

typedef struct ColorRegs {
	Arena *arena;
	IR *ir;
	IRFn *fn;
	Reg *regs;
	RegCtx *ctx;
	Cfg *cfg;
	int16_t *w;
	uint32_t num_regs;
} ColorRegs;

typedef struct CRNode {
	uint64_t *row;
	uint64_t avail;
	uint64_t pref;
	uint32_t vreg;
	uint32_t cost;
	uint32_t cross;
	uint32_t deg;
	uint8_t state;
} CRNode;

typedef struct Graph {
	ColorRegs *cr;
	CRNode *nodes;
	uint64_t *adj;
	uint64_t *live;
	uint64_t *vlive;
	uint32_t *id;
	uint32_t *root;
	uint32_t *stack;
	uint32_t *cfg_first;
	uint32_t *order;

	uint64_t pal;
	uint64_t bpal;
	uint32_t k;
	uint32_t cap;
	uint32_t stride;

	uint32_t node_num;
	uint32_t stack_num;
	uint32_t blk;
} Graph;

void make_graph (ColorRegs *cr, Graph *g);
void color_graph (Graph *g);
void build_graph (Graph *g);
void paint_graph (Graph *g);

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg, uint32_t reg_num);

#endif
