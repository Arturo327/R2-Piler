#include "codegen/regalloc/common.h"
#include "codegen/codegen.h"
#include "ir/cfg.h"

#include <string.h>

#define MAX_HOMES_SIZE (4u << 20)

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
} RegAlloc;

static void count_defs (IR *ir, IRFn *fn, Reg *regs)
{
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t *src[3];

		if (in->op == IR_NOP) continue;
		int n = get_srcs(in, src);

		for (int j = 0; j < n; j++)
			regs[*src[j]].uses++;
		if (in->dst != NO_REG && regs[in->dst].defs < 2)
			regs[in->dst].defs++;
	}
}

static void handle_alias (RegAlloc *ra, uint32_t i, uint8_t *ok, IRInstr *in, int spill_all)
{
	IRFn *fn = ra->fn;
	Reg *regs = ra->regs;
	uint32_t d = in->dst;
	uint32_t s = in->src1;

	if (in->op != IR_MOVE || !same_rep(fn, d, s)) return;
	if (regs[s].cstate == VR_CONST) {
		if (i >= ra->entry && !ok[d]) return;
		regs[d].cstate = VR_CONST;
		regs[d].cval = regs[s].cval;
		return;
	}
	if (spill_all) return;

	if (s >= fn->param_count || regs[s].cstate != VR_MEM) return;
	if (regs[s].defs != 1) return;

	regs[d].cstate = VR_ALIAS;
	regs[d].slot = s;
}

static void mark_const_ok (RegAlloc *ra, uint32_t entry, uint8_t *ok, uint32_t *def_at)
{
	IRFn *fn = ra->fn;

	for (uint32_t r = 0; r < fn->reg_count; r++) {
		ok[r] = 1;
		def_at[r] = 0;
	}
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ra->ir->instrs + fn->start + i;

		if (in->op != IR_NOP && in->dst != NO_REG)
			def_at[in->dst] = i;
	}
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ra->ir->instrs + fn->start + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		for (int k = 0; k < n; k++) {
			uint32_t v = *src[k];
			uint32_t d = def_at[v];

			if (ra->regs[v].defs != 1 || d < entry) continue;
			if (i > d && ra->ctx->ins_block[i] == ra->ctx->ins_block[d])
				continue;
			ok[v] = 0;
		}
	}
}

static void clasf_regs (RegAlloc *ra, int spill_all, uint32_t entry)
{
	IR *ir = ra->ir;
	IRFn *fn = ra->fn;
	Reg *regs = ra->regs;

	if (!fn->reg_count) return;
	ra->entry = entry;
	uint8_t *ok = arena_alloc(ra->arena, fn->reg_count);
	uint32_t *def_at = arena_alloc(ra->arena, (size_t)fn->reg_count * sizeof(uint32_t));
	mark_const_ok(ra, entry, ok, def_at);

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_NOP || in->dst == NO_REG) continue;
		regs[in->dst].cstate = VR_MEM;
		if (regs[in->dst].defs != 1) continue;

		if (in->op == IR_CONST && (i < entry || ok[in->dst])) {
			regs[in->dst].cstate = VR_CONST;
			regs[in->dst].cval = in->imm64;
			continue;
		}

		regs[in->dst].cstate = VR_MEM;
		handle_alias(ra, i, ok, in, spill_all);
	}
}

static void alloc_homes (Arena *arena, IRFn *fn, Reg *regs, uint32_t block_count)
{
	if (!fn->reg_count) return;
	int8_t *homes = arena_alloc(arena, (size_t)fn->reg_count * block_count);
	for (uint32_t r = 0; r < fn->reg_count; r++)
		regs[r].home = homes + (size_t)r * block_count;
}

static void set_home (Reg *reg, uint32_t block_count, int h)
{
	for (uint32_t b = 0; b < block_count; b++)
		reg->home[b] = (int8_t)h;
}

static void default_homes (IRFn *fn, Reg *regs, uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate == VR_CONST || regs[r].cstate == VR_ALIAS)
			set_home(regs + r, block_count, VR_NO_HOME);
		else if (!regs[r].uses)
			set_home(regs + r, block_count, VR_NO_HOME);
		else {
			set_home(regs + r, block_count, VR_SPILL);
			regs[r].spill = 1;
		}
	}
}

static int is_spilled (Reg *reg, uint32_t block_count)
{
	for (uint32_t b = 0; b < block_count; b++)
		if (reg->home[b] == VR_SPILL) return 1;
	return 0;
}

static uint32_t lay_size (IRFn *fn, Reg *regs, uint32_t c, uint8_t s, uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (!is_spilled(regs + r, block_count)) continue;
		if (types[fn->reg_types[r]].size != s) continue;
		c += s;
		regs[r].slot = c;
	}
	return c;
}

static void lay_sizes (IRFn *fn, Reg *regs, uint32_t block_count)
{
	uint32_t c = 0;
	c = lay_size(fn, regs, c, 8, block_count);
	c = lay_size(fn, regs, c, 4, block_count);
	c = lay_size(fn, regs, c, 2, block_count);
	lay_size(fn, regs, c, 1, block_count);
}

static void resolve (IRFn *fn, Reg *regs, uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate != VR_ALIAS) continue;
		uint32_t t = regs[r].slot;
		regs[r].slot = regs[t].slot;
		for (uint32_t b = 0; b < block_count; b++)
			regs[r].home[b] = regs[t].home[b];
	}
}

static void grow_moves (RegAlloc *ra)
{
	uint32_t cap = ra->move_cap ? ra->move_cap * 2 : 64;
	size_t old_size = (size_t)ra->move_cap * sizeof(EdgeMove);
	size_t new_size = (size_t)cap * sizeof(EdgeMove);

	ra->moves = arena_realloc(ra->arena, ra->moves, old_size, new_size);
	ra->move_cap = cap;
}

static int edge_move_seen (RegAlloc *ra, uint32_t first, int8_t from, int8_t to)
{
	for (uint32_t i = first; i < ra->move_count; i++) {
		EdgeMove *m = ra->moves + i;

		if (m->from == from && m->to == to) return 1;
	}
	return 0;
}

static void add_edge_move (RegAlloc *ra, uint32_t first, uint32_t vreg, int8_t from, int8_t to)
{
	if (from == to || from == VR_NO_HOME || to == VR_NO_HOME) return;
	if (edge_move_seen(ra, first, from, to)) return;

	if (ra->move_count == ra->move_cap) grow_moves(ra);

	EdgeMove *m = ra->moves + ra->move_count++;
	m->vreg = vreg;
	m->from = from;
	m->to = to;
}

static void live_bit_move (RegAlloc *ra, uint32_t first, uint32_t v, uint32_t from, uint32_t to)
{
	if (v >= ra->fn->reg_count || ra->regs[v].cstate == VR_CONST) return;
	add_edge_move(ra, first, v, ra->regs[v].home[from], ra->regs[v].home[to]);
}

static uint32_t live_edge_moves (RegAlloc *ra, uint32_t from, uint32_t to, uint32_t cfg_block)
{
	uint32_t first = ra->move_count;
	uint64_t *live = ra->cfg.blocks[cfg_block].live_in;

	for (uint32_t w = 0; w < ra->words; w++) {
		uint64_t bits = live[w];

		while (bits) {
			uint32_t v = (w << 6) + (uint32_t)__builtin_ctzll(bits);

			bits &= bits - 1;
			live_bit_move(ra, first, v, from, to);
		}
	}
	return ra->move_count - first;
}

static uint32_t edge_to_block (RegAlloc *ra, uint32_t from, uint32_t cfg_block)
{
	uint32_t at = ra->cfg.blocks[cfg_block].start - ra->fn->start;
	uint32_t to = ra->ctx->ins_block[at];

	if (to == from) return 0;
	return live_edge_moves(ra, from, to, cfg_block);
}

static void add_taken_edge (RegAlloc *ra, uint32_t at, uint32_t from, IRInstr *last)
{
	uint32_t t = cfg_label_block(ra->ir, &ra->cfg, last->target);
	if (t != CFG_NO_BLOCK) ra->ctx->edges[at].taken = edge_to_block(ra, from, t);
}

static void build_block_edges (RegAlloc *ra, uint32_t b)
{
	CfgBlock *blk = ra->cfg.blocks + b;
	if (!blk->count) return;

	uint32_t at = blk->start - ra->fn->start + blk->count - 1;
	IRInstr *last = ra->ir->instrs + blk->start + blk->count - 1;
	EdgeMoves *e = ra->ctx->edges + at;
	uint32_t from = ra->ctx->ins_block[at];

	e->first = ra->move_count;
	if (last->op != IR_JMP && last->op != IR_RET && b + 1 < ra->cfg.block_count)
		e->fall = edge_to_block(ra, from, b + 1);
	if (last->op == IR_JMP || last->op == IR_JZ || last->op == IR_JNZ)
		add_taken_edge(ra, at, from, last);
}

static void build_edges (RegAlloc *ra)
{
	if (!ra->words) return;
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		build_block_edges(ra, b);
	ra->ctx->moves = ra->moves;
	ra->ctx->move_count = ra->move_count;
}

static void alloc_regalloc (RegAlloc *ra)
{
	uint32_t count = ra->fn->count ? ra->fn->count : 1;
	uint32_t labels = ra->ir->label_count + 1;

	ra->blocks = arena_alloc(ra->arena, (size_t)count * sizeof(CfgBlock));
	ra->lmap = arena_alloc(ra->arena, (size_t)labels * sizeof(uint32_t));
	ra->lrefs = arena_alloc(ra->arena, (size_t)labels * sizeof(uint32_t));
	ra->live = arena_alloc(ra->arena, (size_t)count * ra->words * sizeof(uint64_t));
	ra->tmp = arena_alloc(ra->arena, (size_t)ra->words * sizeof(uint64_t));
	memset(ra->lmap, 0xFF, (size_t)labels * sizeof(uint32_t));
}

static void build_cfg (RegAlloc *ra)
{
	Cfg *cfg = &ra->cfg;

	cfg->blocks = ra->blocks;
	cfg->lmap = ra->lmap;
	cfg->lrefs = ra->lrefs;
	cfg->work = NULL;
	cfg_fill_blocks(ra->ir, ra->fn, cfg->blocks, &cfg->block_count);
	cfg_build_lmap(ra->ir, cfg);
	cfg_build_edges(ra->ir, cfg);
	cfg_live_solve_full(ra->ir, cfg, ra->live, ra->tmp, ra->words);
}

static void partition (RegAlloc *ra)
{
	RegCtx *ctx = ra->ctx;
	uint32_t count = ra->fn->count;

	ctx->block_count = ra->cfg.block_count;
	for (uint32_t b = 0; b < ra->cfg.block_count; b++) {
		CfgBlock *blk = ra->blocks + b;
		uint32_t base = blk->start - ra->fn->start;

		for (uint32_t k = 0; k < blk->count; k++)
			ctx->ins_block[base + k] = b;
	}

	while (ctx->block_count > 1 && (uint64_t)ra->fn->reg_count * ctx->block_count > MAX_HOMES_SIZE) {
		ctx->block_count = (ctx->block_count + 1) / 2;
		for (uint32_t i = 0; i < count; i++)
			ctx->ins_block[i] /= 2;
	}
}

static uint64_t homes_word (RegAlloc *ra, uint64_t bits, uint32_t w, uint32_t blk)
{
	uint64_t m = 0;

	while (bits) {
		uint32_t r = (w << 6) + (uint32_t)__builtin_ctzll(bits);
		int h = ra->regs[r].home[blk];

		if (h >= 0) m |= 1ull << h;
		bits &= bits - 1;
	}
	return m;
}

static uint64_t homes_live (RegAlloc *ra, const uint64_t *live, uint32_t blk)
{
	uint64_t m = 0;
	for (uint32_t w = 0; w < ra->words; w++)
		m |= homes_word(ra, live[w], w, blk);
	return m;
}

static uint64_t callee_homes (RegAlloc *ra)
{
	uint64_t m = 0;

	for (uint32_t r = 0; r < ra->fn->reg_count; r++) {
		for (uint32_t b = 0; b < ra->ctx->block_count; b++) {
			int h = ra->regs[r].home[b];
			if (h >= 0) m |= 1ull << h;
		}
	}
	return m & ra->ctx->callee_mask;
}

static void sweep_block (RegAlloc *ra, uint32_t b)
{
	CfgBlock *blk = ra->cfg.blocks + b;
	IRInstr *code = ra->ir->instrs + blk->start;
	uint32_t base = blk->start - ra->fn->start;
	uint64_t caller = ~ra->ctx->callee_mask;
	uint64_t allowed = caller | ra->ctx->callee_used;

	cfg_live_out(&ra->cfg, b, ra->tmp, ra->words);
	for (uint32_t i = blk->count; i-- > 0;) {
		IRInstr *in = code + i;
		uint32_t at = base + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		if (in->dst != NO_REG)
			ra->tmp[in->dst >> 6] &= ~(1ull << (in->dst & 63));
		uint64_t hm = homes_live(ra, ra->tmp, ra->ctx->ins_block[at]);
		ra->ctx->free_regs[at] = ra->all & ~hm & allowed;

		if (in->op == IR_CALL &&
				!cg_is_tail_call(ra->ir, ra->fn, ra->regs, ra->ctx, in, ra->ctx->arg_reg_max))
			ra->ctx->save[at] = hm & caller;
		for (int k = 0; k < n; k++)
			ra->tmp[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	}
}

static void sweep_fn (RegAlloc *ra)
{
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		sweep_block(ra, b);
}

static void reset_regs (IRFn *fn, Reg *regs)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		regs[r].uses = 0;
		regs[r].slot = 0;
		regs[r].cval = 0;
		regs[r].cstate = VR_NONE;
		regs[r].defs = 0;
		regs[r].spill = 0;
	}
}

static void make_ctx (IRFn *fn, RegCtx *ctx, uint32_t reg_num)
{
	uint64_t all = ~0ull >> (64 - reg_num);

	memset(ctx->edges, 0, ((size_t)fn->count + 1) * sizeof(EdgeMoves));
	for (uint32_t i = 0; i < fn->count; i++) {
		ctx->ins_block[i] = 0;
		ctx->save[i] = 0;
		ctx->free_regs[i] = all & ~ctx->callee_mask;
	}
	ctx->block_count = 1;
	ctx->callee_used = 0;
	ctx->moves = NULL;
	ctx->move_count = 0;
}

static void init_state (RegAlloc *ra, Arena *arena, IR *ir, IRFn *fn,
		Reg *regs, RegCtx *ctx, OpNeeds *needs, uint32_t reg_num)
{
	reset_regs(fn, regs);
	make_ctx(fn, ctx, reg_num);

	ra->arena = arena;
	ra->ir = ir;
	ra->fn = fn;
	ra->regs = regs;
	ra->ctx = ctx;
	ra->moves = NULL;
	ra->move_cap = 0;
	ra->move_count = 0;
	ra->all = ~0ull >> (64 - reg_num);
	ra->words = (fn->reg_count + 63) >> 6;
	ctx->needs = needs;

	if (!ra->words) return;
	alloc_regalloc(ra);
	build_cfg(ra);
	partition(ra);
}

static void fill_ctx (RegAlloc *ra)
{
	if (!ra->words) return;
	ra->ctx->callee_used = callee_homes(ra);
	sweep_fn(ra);
}

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx,
		uint32_t reg_num, OpNeeds *needs, int spill_all)
{
	RegAlloc ra;
	init_state(&ra, arena, ir, fn, regs, ctx, needs, reg_num);

	alloc_homes(arena, fn, regs, ctx->block_count);
	count_defs(ir, fn, regs);
	clasf_regs(&ra, spill_all, cfg_entry_len(ir, fn));

	default_homes(fn, regs, ctx->block_count);
	if (!spill_all) color_regs(arena, ir, fn, regs, ctx, &ra.cfg);
	lay_sizes(fn, regs, ctx->block_count);

	resolve(fn, regs, ctx->block_count);
	build_edges(&ra);
	fill_ctx(&ra);
}
