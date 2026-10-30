#ifndef RA_COMMON_H
#define RA_COMMON_H

#include <stdint.h>

#include "ir/cfg.h"
#include "codegen/regalloc/regalloc.h"
#include "codegen/codegen.h"

#define GRAPH_MAX_NODES 1024u
#define GRAPH_NO_NODE 0xFFFFFFFFu

enum {
	CRN_ACTIVE = 0,
	CRN_STACKED,
	CRN_MERGED
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
	uint64_t hint;
	uint64_t forbid;
	uint32_t rep;
	uint32_t vreg;
	uint32_t cost;
	uint32_t cross;
	uint32_t deg;
	uint8_t state;
} CRNode;

typedef struct Affinity {
	uint32_t a;
	uint32_t b;
	uint32_t wt;
} Affinity;

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

	uint32_t *nb_first;
	uint32_t *nb_cur;
	uint32_t *nb_part;
	uint32_t *nb_wt;
	uint8_t *done;
	Affinity *aff;

	uint64_t pal;
	uint64_t bpal;
	uint64_t fpal;
	uint64_t rsv;

	uint32_t k;
	uint32_t cap;
	uint32_t stride;

	uint32_t node_num;
	uint32_t stack_num;
	uint32_t aff_num;
	uint32_t merged;
	uint32_t blk;
} Graph;

typedef struct RegAlloc {
	Arena *arena;
	IR *ir;
	IRFn *fn;
	Reg *regs;
	RegCtx *ctx;

	CfgBlock *blocks;
	uint32_t *lmap;
	uint32_t *lrefs;
	uint64_t *live;
	uint64_t *tmp;

	EdgeMove *moves;
	uint32_t move_cap;
	uint32_t move_count;

	Cfg cfg;
	uint64_t all;
	uint32_t words;
	uint32_t entry;

	uint8_t spill_all;
} RegAlloc;

void make_graph (ColorRegs *cr, Graph *g);
void color_graph (Graph *g);
void build_graph (Graph *g);
void paint_graph (Graph *g);

void set_palette (Graph *g, uint32_t scratch);
void reset_nodes (Graph *g);
uint32_t block_scratch (Graph *g);

void coalesce_graph (Graph *g);
uint32_t node_find (Graph *g, uint32_t id);

uint64_t needs_mask (const ArchDesc *arch, Reg *regs, IRInstr *in);
void verify_regalloc (RegAlloc *ra);

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg);

#endif
