#include "codegen/regalloc.h"
#include "ir/cfg.h"

#include <string.h>

#define MAX_HOMES_SIZE (4u << 20)
#define LOOP_WEIGHT 8u
#define MIN_HOME_SCORE 3u
#define MAX_CALLER_HOMES 2
#define MAX_CALLEE_HOMES 5
#define MAX_HOMES (MAX_CALLER_HOMES + MAX_CALLEE_HOMES)

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
	uint8_t *cross;

	Cfg cfg;
	uint64_t all;
	uint32_t words;
	uint32_t entry;
} RegAlloc;

static void reset_regalloc (IRFn *fn, Reg *regs)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		regs[r].uses = 0;
		regs[r].wscore = 0;
		regs[r].slot = 0;
		regs[r].cval = 0;
		regs[r].cstate = VR_NONE;
		regs[r].defs = 0;
		regs[r].spill = 0;
	}
}

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

static int tail_self (RegAlloc *ra, uint32_t i)
{
	IR *ir = ra->ir;
	IRFn *fn = ra->fn;
	IRInstr *in = ir->instrs + fn->start + i;
	IRInstr *end = ir->instrs + fn->start + fn->count;
	IRInstr *n = in + 1;
	uint8_t from = ir->fns[in->target].ret_type;
	uint8_t to = fn->ret_type;

	while (n < end && n->op == IR_LABEL) n++;
	if (n >= end || n->op != IR_RET) return 0;
	if (n->src1 != in->dst || in->argc > 6) return 0;
	if (in->dst != NO_REG && ra->regs[in->dst].uses != 1) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

static void loop_weights (RegAlloc *ra, int16_t *w)
{
	IR *ir = ra->ir;
	IRFn *fn = ra->fn;
	uint32_t labels = ir->label_count + 1;
	uint32_t *lpos = arena_alloc(ra->arena, labels * sizeof(uint32_t));
	uint32_t self = (uint32_t)(fn - ir->fns);

	for (uint32_t i = 0; i < labels; i++)
		lpos[i] = UINT32_MAX;
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		if (in->op == IR_LABEL && in->target < labels)
			lpos[in->target] = i;
	}
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t t = UINT32_MAX;

		if (in->op == IR_JMP || in->op == IR_JZ || in->op == IR_JNZ)
			t = in->target < labels ? lpos[in->target] : UINT32_MAX;
		else if (in->op == IR_CALL && in->target == self && tail_self(ra, i))
			t = 0;
		if (t == UINT32_MAX || t > i) continue;
		w[t]++;
		if (i + 1 < fn->count) w[i + 1]--;
	}
	int16_t d = 0;
	for (uint32_t i = 0; i < fn->count; i++) {
		d = (int16_t)(d + w[i]);
		w[i] = d > 0 ? LOOP_WEIGHT : 1;
	}
}

static void weigh_regs (RegAlloc *ra, const int16_t *w)
{
	IR *ir = ra->ir;
	IRFn *fn = ra->fn;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		for (int k = 0; k < n; k++)
			ra->regs[*src[k]].wscore += (uint32_t)w[i];
		if (in->dst != NO_REG)
			ra->regs[in->dst].wscore += (uint32_t)w[i];
	}
}

static void mark_weighted (RegAlloc *ra)
{
	uint32_t count = ra->fn->count ? ra->fn->count : 1;
	int16_t *w = arena_alloc(ra->arena, count * sizeof(int16_t));

	memset(w, 0, count * sizeof(int16_t));
	loop_weights(ra, w);
	weigh_regs(ra, w);
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

typedef struct HomePicks {
	uint32_t pick[MAX_HOMES];
	uint8_t callreg[MAX_CALLER_HOMES];
	uint8_t calreg[MAX_CALLEE_HOMES];
	uint32_t count;
	uint32_t nc;
	uint32_t nn;
} HomePicks;

static uint32_t list_mask (uint64_t mask, uint8_t *regs, uint32_t cap)
{
	uint32_t n = 0;

	while (mask && n < cap) {
		regs[n++] = (uint8_t)__builtin_ctzll(mask);
		mask &= mask - 1;
	}
	return n;
}

static uint32_t rank_picks (IRFn *fn, Reg *regs, uint32_t *pick)
{
	uint32_t n = 0;

	for (uint32_t r = 0; r < fn->reg_count; r++) {
		uint32_t k = 0;

		if (regs[r].cstate != VR_MEM || !regs[r].uses) continue;
		if (regs[r].wscore < MIN_HOME_SCORE) continue;
		while (k < n && regs[pick[k]].wscore >= regs[r].wscore) k++;
		if (k == MAX_HOMES) continue;

		uint32_t j = n < MAX_HOMES ? n : MAX_HOMES - 1;
		while (j > k) {
			pick[j] = pick[j - 1];
			j--;
		}
		pick[k] = r;
		if (n < MAX_HOMES) n++;
	}
	return n;
}

static void pick_homes (IRFn *fn, Reg *regs, RegCtx *ctx, HomePicks *hp)
{
	hp->nc = list_mask(ctx->caller_mask, hp->callreg, MAX_CALLER_HOMES);
	hp->nn = list_mask(ctx->callee_mask, hp->calreg, MAX_CALLEE_HOMES);
	hp->count = rank_picks(fn, regs, hp->pick);
}

static void apply_homes (Reg *regs, uint32_t block_count,
		const uint8_t *cross, const HomePicks *hp)
{
	uint32_t ci = 0, li = 0;

	for (uint32_t k = 0; k < hp->count; k++) {
		uint32_t r = hp->pick[k];

		if (!cross[r] && ci < hp->nc)
			set_home(regs + r, block_count, hp->callreg[ci++]);
		else if (li < hp->nn)
			set_home(regs + r, block_count, hp->calreg[li++]);
	}
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

static void alloc_regalloc (RegAlloc *ra)
{
	uint32_t count = ra->fn->count ? ra->fn->count : 1;
	uint32_t labels = ra->ir->label_count + 1;

	ra->blocks = arena_alloc(ra->arena, (size_t)count * sizeof(CfgBlock));
	ra->lmap = arena_alloc(ra->arena, (size_t)labels * sizeof(uint32_t));
	ra->lrefs = arena_alloc(ra->arena, (size_t)labels * sizeof(uint32_t));
	ra->live = arena_alloc(ra->arena,
			(size_t)count * ra->words * sizeof(uint64_t));
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

static void cross_call (RegAlloc *ra, uint8_t *cross, uint32_t v)
{
	uint32_t root = ra->regs[v].cstate == VR_ALIAS ? ra->regs[v].slot : v;

	cross[v] = 1;
	cross[root] = 1;
}

static void cross_instr (RegAlloc *ra, uint8_t *cross)
{
	for (uint32_t w = 0; w < ra->words; w++) {
		uint64_t bits = ra->tmp[w];

		while (bits) {
			cross_call(ra, cross, (w << 6) + (uint32_t)__builtin_ctzll(bits));
			bits &= bits - 1;
		}
	}
}

static void cross_block (RegAlloc *ra, uint32_t b, uint8_t *cross)
{
	CfgBlock *blk = ra->cfg.blocks + b;
	IRInstr *code = ra->ir->instrs + blk->start;

	cfg_live_out(&ra->cfg, b, ra->tmp, ra->words);
	for (uint32_t i = blk->count; i-- > 0;) {
		IRInstr *in = code + i;
		uint32_t *src[3];
		int n = get_srcs(in, src);

		if (in->dst != NO_REG)
			ra->tmp[in->dst >> 6] &= ~(1ull << (in->dst & 63));
		if (in->op == IR_CALL) cross_instr(ra, cross);
		for (int k = 0; k < n; k++)
			ra->tmp[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	}
}

static void mark_crossing (RegAlloc *ra, uint8_t *cross)
{
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		cross_block(ra, b, cross);
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

		if (in->op == IR_CALL) ra->ctx->save[at] = hm & caller;
		for (int k = 0; k < n; k++)
			ra->tmp[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	}
}

static void sweep_fn (RegAlloc *ra)
{
	for (uint32_t b = 0; b < ra->cfg.block_count; b++)
		sweep_block(ra, b);
}

static void init_state (RegAlloc *ra, Arena *arena, IR *ir,
		IRFn *fn, Reg *regs, RegCtx *ctx, uint32_t reg_num)
{
	ra->arena = arena;
	ra->ir = ir;
	ra->fn = fn;
	ra->regs = regs;
	ra->ctx = ctx;
	ra->all = ~0ull >> (64 - reg_num);
	ra->cross = NULL;
	ra->words = (fn->reg_count + 63) >> 6;
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

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs,
		RegCtx *ctx, uint32_t reg_num, int spill_all)
{
	reset_regalloc(fn, regs);
	make_ctx(fn, ctx, reg_num);

	RegAlloc ra;
	init_state(&ra, arena, ir, fn, regs, ctx, reg_num);

	alloc_homes(arena, fn, regs, ctx->block_count);
	count_defs(ir, fn, regs);
	if (!spill_all && fn->reg_count) mark_weighted(&ra);
	clasf_regs(&ra, spill_all, cfg_entry_len(ir, fn));

	if (!spill_all) {
		uint32_t csz = fn->reg_count ? fn->reg_count : 1;
		ra.cross = arena_alloc(arena, csz);
		memset(ra.cross, 0, csz);
		if (fn->reg_count) mark_crossing(&ra, ra.cross);
	}

	default_homes(fn, regs, ctx->block_count);
	if (!spill_all) {
		HomePicks hp;
		pick_homes(fn, regs, ctx, &hp);
		apply_homes(regs, ctx->block_count, ra.cross, &hp);
	}
	lay_sizes(fn, regs, ctx->block_count);

	resolve(fn, regs, ctx->block_count);
	fill_ctx(&ra);
}
