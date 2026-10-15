#include "codegen/regalloc.h"

static void reset_regalloc (IRFn *fn, Reg *regs)
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

static void handle_alias (IRFn *fn, Reg *regs, IRInstr *in, int spill_all)
{
	uint32_t d = in->dst;
	uint32_t s = in->src1;

	if (in->op != IR_MOVE || !same_rep(fn, d, s)) return;
	if (regs[s].cstate == VR_CONST) {
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

static void clasf_regs (IR *ir, IRFn *fn, Reg *regs, int spill_all)
{
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_NOP || in->dst == NO_REG) continue;
		regs[in->dst].cstate = VR_MEM;
		if (regs[in->dst].defs != 1) continue;

		if (in->op == IR_CONST) {
			regs[in->dst].cstate = VR_CONST;
			regs[in->dst].cval = in->imm64;
			continue;
		}

		regs[in->dst].cstate = VR_MEM;
		handle_alias(fn, regs, in, spill_all);
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

static void assign_homes (IRFn *fn, Reg *regs, uint32_t block_count)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate == VR_CONST || regs[r].cstate == VR_ALIAS) {
			set_home(regs + r, block_count, VR_NO_HOME);
			continue;
		}
		if (!regs[r].uses) {
			set_home(regs + r, block_count, VR_NO_HOME);
			continue;
		}
		set_home(regs + r, block_count, VR_SPILL);
		regs[r].spill = 1;
	}
}

static int is_spilled (Reg *reg, uint32_t block_count)
{
	for (uint32_t b = 0; b < block_count; b++)
		if (reg->home[b] == VR_SPILL) return 1;
	return 0;
}

static uint32_t lay_size (IRFn *fn, Reg *regs, uint32_t c, uint8_t s,
		uint32_t block_count)
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
	uint16_t all = (uint16_t)((1u << reg_num) - 1);

	for (uint32_t i = 0; i < fn->count; i++) {
		ctx->ins_block[i] = 0;
		ctx->save[i] = 0;
		ctx->free_regs[i] = all & (uint16_t)~ctx->callee_mask;
	}
	ctx->block_count = 1;
	ctx->callee_used = 0;
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

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs,
		RegCtx *ctx, uint32_t reg_num, int spill_all)
{
	reset_regalloc(fn, regs);
	make_ctx(fn, ctx, reg_num);
	alloc_homes(arena, fn, regs, ctx->block_count);

	count_defs(ir, fn, regs);
	clasf_regs(ir, fn, regs, spill_all);

	assign_homes(fn, regs, ctx->block_count);
	lay_sizes(fn, regs, ctx->block_count);

	resolve(fn, regs, ctx->block_count);
}
