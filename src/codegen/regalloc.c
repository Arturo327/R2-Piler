#include "codegen/regalloc.h"

static void reset_regalloc (IRFn *fn, Reg *regs)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		regs[r].uses = 0;
		regs[r].slot = 0;
		regs[r].cval = 0;
		regs[r].cstate = RA_NONE;
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

static void handle_alias (IRFn *fn, Reg *regs, IRInstr *in)
{
	uint32_t d = in->dst;
	uint32_t s = in->src1;

	if (in->op != IR_MOVE || !same_rep(fn, d, s)) return;
	if (regs[s].cstate == RA_CONST) {
		regs[d].cstate = RA_CONST;
		regs[d].cval = regs[s].cval;
		return;
	}

	if (s >= fn->param_count || regs[s].cstate != RA_MEM) return;
	if (regs[s].defs != 1) return;

	regs[d].cstate = RA_ALIAS;
	regs[d].slot = s;
}

static void clasf_regs (IR *ir, IRFn *fn, Reg *regs, int spill_all)
{
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op == IR_NOP || in->dst == NO_REG) continue;
		if (regs[in->dst].defs != 1) continue;

		if (in->op == IR_CONST) {
			regs[in->dst].cstate = RA_CONST;
			regs[in->dst].cval = in->imm64;
			continue;
		}
		if (spill_all) {
			regs[in->dst].cstate = RA_MEM;
			continue;
		}

		regs[in->dst].cstate = RA_MEM;
		handle_alias(fn, regs, in);
	}
}

static void assign_homes (IRFn *fn, Reg *regs)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate == RA_CONST || regs[r].cstate == RA_ALIAS) {
			regs[r].home[0] = RA_NO_HOME;
			continue;
		}
		if (!regs[r].uses) {
			regs[r].home[0] = RA_NO_HOME;
			continue;
		}
		regs[r].home[0] = RA_SPILL;
		regs[r].spill = 1;
	}
}

static uint32_t lay_size (IRFn *fn, Reg *regs, uint32_t c, uint8_t s)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].home[0] != RA_SPILL) continue;
		if (types[fn->reg_types[r]].size != s) continue;
		c += s;
		regs[r].slot = c;
	}
	return c;
}

static void lay_sizes (IRFn *fn, Reg *regs)
{
	uint32_t c = 0;
	c = lay_size(fn, regs, c, 8);
	c = lay_size(fn, regs, c, 4);
	c = lay_size(fn, regs, c, 2);
	lay_size(fn, regs, c, 1);
}

static void make_meta (IRFn *fn, RaMeta *m)
{
	for (uint32_t i = 0; i < fn->count; i++)
		m->ins_block[i] = 0;
	m->block_count = 1;
	m->save[0] = 0;
	m->callee_used = 0;
}

static void resolve (IRFn *fn, Reg *regs)
{
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (regs[r].cstate != RA_ALIAS) continue;
		regs[r].slot = regs[regs[r].slot].slot;
	}
}

void regalloc (Arena *arena, IR *ir, IRFn *fn, Reg *regs,
		RaMeta *meta, uint32_t reg_num, int spill_all)
{
	(void)arena;
	(void)reg_num;

	reset_regalloc(fn, regs);
	count_defs(ir, fn, regs);
	clasf_regs(ir, fn, regs, spill_all);

	assign_homes(fn, regs);
	lay_sizes(fn, regs);

	resolve(fn, regs);
	make_meta(fn, meta);
}
