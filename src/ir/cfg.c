#include "ir/cfg.h"
#include "ir/opt.h"

#include <string.h>

static int starts_block (IR *ir, IRFn *fn, uint32_t i)
{
	if (!i) return 1;

	uint8_t prev = ir->instrs[fn->start + i - 1].op;
	if (prev == IR_JMP || prev == IR_JZ || prev == IR_JNZ || prev == IR_RET)
		return 1;

	uint8_t cur = ir->instrs[fn->start + i].op;
	if (cur == IR_LABEL) return 1;
	return 0;
}

void cfg_fill_blocks (IR *ir, IRFn *fn, CfgBlock *blocks, uint32_t *block_count)
{
	uint32_t b = 0;

	blocks[0].start = fn->start;
	blocks[0].count = 0;
	for (uint32_t i = 0; i < fn->count; i++) {
		if (i > 0 && starts_block(ir, fn, i)) {
			b++;
			blocks[b].start = fn->start + i;
			blocks[b].count = 0;
		}
		blocks[b].count++;
	}
	*block_count = b + 1;
}

uint32_t cfg_entry_len (IR *ir, IRFn *fn)
{
	uint32_t n = 0;

	while (n < fn->count) {
		uint8_t op = ir->instrs[fn->start + n].op;

		if (op == IR_LABEL || op == IR_JMP || op == IR_JZ
				|| op == IR_JNZ || op == IR_RET)
			break;
		n++;
	}
	return n;
}

void cfg_build_lmap (IR *ir, Cfg *cfg)
{
	for (uint32_t j = 0; j < cfg->block_count; j++) {
		IRInstr *in = ir->instrs + cfg->blocks[j].start;

		if (in->op != IR_LABEL) continue;
		cfg->lmap[in->target] = j;
		cfg->lrefs[in->target] = 0;
	}
}

uint32_t cfg_label_block (IR *ir, Cfg *cfg, uint32_t label)
{
	uint32_t b = cfg->lmap[label];
	if (b >= cfg->block_count) return CFG_NO_BLOCK;

	IRInstr *in = ir->instrs + cfg->blocks[b].start;
	return in->op == IR_LABEL && in->target == label ? b : CFG_NO_BLOCK;
}

void cfg_build_edges (IR *ir, Cfg *cfg)
{
	for (uint32_t b = 0; b < cfg->block_count; b++) {
		CfgBlock *blk = cfg->blocks + b;
		IRInstr *last;
		int jumps, falls;

		blk->succ_count = 0;
		blk->reachable = 0;
		if (!blk->count) continue;

		last = ir->instrs + blk->start + blk->count - 1;
		jumps = last->op == IR_JMP || last->op == IR_JZ || last->op == IR_JNZ;
		falls = last->op != IR_JMP && last->op != IR_RET && b + 1 < cfg->block_count;
		if (falls) blk->succ[blk->succ_count++] = b + 1;
		if (jumps) {
			uint32_t t = cfg_label_block(ir, cfg, last->target);
			if (t != CFG_NO_BLOCK) blk->succ[blk->succ_count++] = t;
		}
	}
}

void cfg_mark_reachable (Cfg *cfg)
{
	uint32_t sp = 0;

	cfg->work[sp++] = 0;
	cfg->blocks[0].reachable = 1;
	while (sp) {
		CfgBlock *b = cfg->blocks + cfg->work[--sp];

		for (uint32_t k = 0; k < b->succ_count; k++) {
			if (cfg->blocks[b->succ[k]].reachable) continue;
			cfg->blocks[b->succ[k]].reachable = 1;
			cfg->work[sp++] = b->succ[k];
		}
	}
}

int cfg_live_step (IRInstr *in, uint64_t *live)
{
	uint32_t *src[3];
	int n = get_srcs(in, src);

	if (in->dst != NO_REG) {
		uint64_t bit = 1ull << (in->dst & 63);

		if (!(live[in->dst >> 6] & bit) && is_deletable[in->op]) return 1;
		live[in->dst >> 6] &= ~bit;
	}
	for (int k = 0; k < n; k++)
		live[*src[k] >> 6] |= 1ull << (*src[k] & 63);
	return 0;
}

static void cfg_live_step_full (IRInstr *in, uint64_t *live)
{
	uint32_t *src[3];
	int n = get_srcs(in, src);

	if (in->dst != NO_REG)
		live[in->dst >> 6] &= ~(1ull << (in->dst & 63));
	for (int k = 0; k < n; k++)
		live[*src[k] >> 6] |= 1ull << (*src[k] & 63);
}

void cfg_live_out (Cfg *cfg, uint32_t b, uint64_t *out, uint32_t words)
{
	memset(out, 0, (size_t)words * sizeof(uint64_t));
	for (uint32_t k = 0; k < cfg->blocks[b].succ_count; k++) {
		uint64_t *src = cfg->blocks[cfg->blocks[b].succ[k]].live_in;

		for (uint32_t w = 0; w < words; w++)
			out[w] |= src[w];
	}
}

static int live_block (IR *ir, Cfg *cfg, uint32_t b, uint64_t *tmp, uint32_t words)
{
	IRInstr *code = ir->instrs + cfg->blocks[b].start;
	CfgBlock *blk = cfg->blocks + b;
	int changed = 0;

	cfg_live_out(cfg, b, tmp, words);
	for (uint32_t i = blk->count; i-- > 0;) {
		if (cfg->live_full) cfg_live_step_full(code + i, tmp);
		else cfg_live_step(code + i, tmp);
	}
	for (uint32_t w = 0; w < words; w++) {
		changed |= tmp[w] != blk->live_in[w];
		blk->live_in[w] = tmp[w];
	}
	return changed;
}

static void live_solve (IR *ir, Cfg *cfg, uint64_t *live, uint64_t *tmp, uint32_t words)
{
	uint64_t *p = live;
	int again = 1;

	memset(live, 0, (size_t)cfg->block_count * words * sizeof(uint64_t));
	for (uint32_t b = 0; b < cfg->block_count; b++, p += words)
		cfg->blocks[b].live_in = p;

	while (again) {
		again = 0;
		for (uint32_t b = cfg->block_count; b-- > 0;)
			again |= live_block(ir, cfg, b, tmp, words);
	}
}

void cfg_live_solve (IR *ir, Cfg *cfg, uint64_t *live, uint64_t *tmp, uint32_t words)
{
	cfg->live_full = 0;
	live_solve(ir, cfg, live, tmp, words);
}

void cfg_live_solve_full (IR *ir, Cfg *cfg, uint64_t *live, uint64_t *tmp, uint32_t words)
{
	cfg->live_full = 1;
	live_solve(ir, cfg, live, tmp, words);
}

void opt_cfg (OptFn *f, Cfg *cfg)
{
	cfg->blocks = f->blocks;
	cfg->lmap = f->lmap;
	cfg->lrefs = f->lrefs;
	cfg->work = f->work;
	cfg->block_count = f->block_count;
}
