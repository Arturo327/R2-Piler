#include "ir/opts/common.h"

#define OPT_LIVE_MAX_WORDS (1u << 22)

int opt_dce (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = f->fn->count; i-- > 0;) {
		IRInstr *in = code + i;

		if (in->dst == NO_REG || f->uses[in->dst] || !is_deletable[in->op]) continue;

		uint32_t *src[3];
		int n = get_srcs(in, src);
		for (int k = 0; k < n; k++)
			f->uses[*src[k]]--;

		kill_instr(in);
		changed = 1;
	}
	return changed;
}

static int live_sweep (Optimizer *opt, OptFn *f, Cfg *cfg, uint32_t b, uint32_t words)
{
	IRInstr *code = opt->ir->instrs + cfg->blocks[b].start;
	int changed = 0;

	cfg_live_out(cfg, b, f->live_tmp, words);
	for (uint32_t i = cfg->blocks[b].count; i-- > 0;) {
		if (!cfg_live_step(code + i, f->live_tmp)) continue;
		kill_instr(code + i);
		changed = 1;
	}
	return changed;
}

static void live_setup (Optimizer *opt, OptFn *f, size_t need)
{
	if (need > f->live_cap) {
		f->live = arena_alloc(opt->arena, need * sizeof(uint64_t));
		f->live_cap = need;
	}
}

int opt_live_dce (Optimizer *opt, OptFn *f)
{
	uint32_t words = (f->fn->reg_count + 63) >> 6;
	Cfg *cfg = &opt->cfg;
	int changed = 0;

	if (!words) return 0;
	if ((size_t)f->block_count * words > OPT_LIVE_MAX_WORDS) return 0;

	cfg_build_edges(opt->ir, cfg);
	live_setup(opt, f, (size_t)cfg->block_count * words);
	cfg_live_solve(opt->ir, cfg, f->live, f->live_tmp, words);
	for (uint32_t b = 0; b < cfg->block_count; b++)
		changed |= live_sweep(opt, f, cfg, b, words);
	return changed;
}
