#include "codegen/x86-64/common.h"
#include <stdio.h>
#include <stdlib.h>

void x86_move_saved (X86Fn *f, uint64_t mask, int restore)
{
	uint32_t off = f->regsave;

	for (uint32_t k = 0; k < X86_REG_NUM; k++) {
		if (!(f->savemask >> k & 1)) continue;
		off += 8;
		if (!(mask >> k & 1)) continue;

		if (restore)
			cg_printf(f->cg, "\tmovq -%u(%s), %s\n", off, f->base, reg_name(k, 8));
		else cg_printf(f->cg, "\tmovq %s, -%u(%s)\n", reg_name(k, 8), off, f->base);
	}
}

int x86_phys_reg (X86Fn *f, uint32_t vreg)
{
	int8_t h = f->regs[vreg].home[f->blk];

	if (h < 0 || f->regs[vreg].cstate == VR_CONST) return -1;
	return h;
}

uint64_t x86_home_bit (X86Fn *f, uint32_t vreg)
{
	int p = vreg == NO_REG ? -1 : x86_phys_reg(f, vreg);

	return p < 0 ? 0 : 1ull << p;
}

void x86_begin_instr (X86Fn *f, IRInstr *i)
{
	uint32_t *src[3];
	int n = get_srcs(i, src);

	f->at = (uint32_t)(i - (f->cg->ir->instrs + f->fn->start));
	f->blk = f->ctx.ins_block[f->at];
	f->pool = f->ctx.free_regs[f->at];
	f->busy = x86_home_bit(f, i->dst);
	for (int k = 0; k < n; k++)
		f->busy |= x86_home_bit(f, *src[k]);
}

static void regalloc_violation (X86Fn *f, const char *what)
{
	fprintf(stderr, "internal error: regalloc contract broken in '%.*s', instr %u: %s\n",
			(int)f->fn->len, f->fn->name, f->at, what);
	exit(1);
}

uint8_t x86_grab (X86Fn *f)
{
	uint64_t m = f->pool & ~f->busy;
	if (!m) regalloc_violation(f, "no scratch register available");

	uint8_t r = (uint8_t)__builtin_ctzll(m);
	f->pool &= ~(1ull << r);
	return r;
}

void x86_take_fixed (X86Fn *f, uint8_t r, uint32_t keeps)
{
	if (keeps != NO_REG && x86_phys_reg(f, keeps) == r) return;
	if (!(f->pool >> r & 1)) regalloc_violation(f, "fixed register is not free");
	f->pool &= ~(1ull << r);
}

void x86_take_fixed_rw (X86Fn *f, uint8_t r)
{
	if (!(f->pool >> r & 1)) regalloc_violation(f, "fixed register is not free");
	f->pool &= ~(1ull << r);
}

uint8_t x86_work_reg (X86Fn *f, uint32_t dst, uint64_t avoid)
{
	int d = x86_phys_reg(f, dst);

	if (d >= 0 && !(avoid >> d & 1)) return (uint8_t)d;
	return x86_grab(f);
}

static void check_home (X86Fn *f, uint32_t r, int h)
{
	if (h < VR_SPILL || h >= X86_REG_NUM)
		regalloc_violation(f, "vreg home out of range");
	if (h == VR_SPILL && !f->regs[r].slot)
		regalloc_violation(f, "spilled vreg has no stack slot");
	if (h == VR_SPILL && !f->regs[r].spill && f->regs[r].cstate != VR_ALIAS)
		regalloc_violation(f, "spilled vreg is not flagged in Reg.spill");
	if (h >= 0 && (X86_CALLEE_MASK >> h & 1)
			&& !(f->ctx.callee_used >> h & 1))
		regalloc_violation(f, "callee-saved home missing from callee_used");
}

static void check_homes (X86Fn *f)
{
	for (uint32_t r = 0; r < f->fn->reg_count; r++)
		if (!types[f->fn->reg_types[r]].size)
			regalloc_violation(f, "vreg has an unsized type");

	for (uint32_t b = 0; b < f->ctx.block_count; b++)
		for (uint32_t r = 0; r < f->fn->reg_count; r++)
			check_home(f, r, f->regs[r].home[b]);
}

static void check_used_home (X86Fn *f, uint32_t v, uint32_t blk)
{
	Reg *reg = f->regs + v;

	if (reg->cstate == VR_CONST) return;
	if (reg->home[blk] == VR_NO_HOME)
		regalloc_violation(f, "used vreg has no home in this block");
}

static void check_instr_homes (X86Fn *f, IRInstr *in, uint32_t blk)
{
	uint32_t *src[3];
	int n;

	if (in->op == IR_NOP) return;
	n = get_srcs(in, src);
	for (int k = 0; k < n; k++)
		check_used_home(f, *src[k], blk);
	if (in->dst == NO_REG || !f->regs[in->dst].uses) return;
	if (f->regs[in->dst].cstate != VR_MEM) return;
	check_used_home(f, in->dst, blk);
}

static void check_cstate (X86Fn *f, IRInstr *in)
{
	uint8_t c;

	if (in->dst == NO_REG) return;
	c = f->regs[in->dst].cstate;
	if ((c == VR_CONST || c == VR_ALIAS) && !is_deletable[in->op])
		regalloc_violation(f, "CONST/ALIAS cstate on a non-deletable instruction");
}

static void check_save (X86Fn *f, IRInstr *in, uint32_t i)
{
	uint64_t s = f->saves[i];

	if (s & X86_CALLEE_MASK)
		regalloc_violation(f, "save[] lists a callee-saved home");
	if (in->op != IR_CALL) {
		if (s) regalloc_violation(f, "save[] set outside a call");
		return;
	}
	if (s & x86_home_bit(f, in->dst))
		regalloc_violation(f, "save[] contains the home of the call's dst");
	if (s && cg_is_tail_call(f->cg->ir, f->fn, f->regs, &f->ctx, in, f->ctx.tail_argc_max))
		regalloc_violation(f, "save[] set on a tail call");
}

static void check_ctx (X86Fn *f)
{
	IRInstr *code = f->cg->ir->instrs + f->fn->start;

	for (uint32_t i = 0; i < f->fn->count; i++) {
		f->at = i;
		f->blk = f->ctx.ins_block[i];
		if (f->blk >= f->ctx.block_count)
			regalloc_violation(f, "ins_block out of range");
		if (f->frees[i] & X86_CALLEE_MASK & ~f->ctx.callee_used)
			regalloc_violation(f, "free_regs lists a callee-saved reg not in callee_used");
		check_instr_homes(f, code + i, f->blk);
		check_cstate(f, code + i);
		check_save(f, code + i, i);
	}
}

void x86_check_regalloc (X86Fn *f)
{
	f->at = 0;
	f->blk = 0;
	check_homes(f);
	check_ctx(f);
}

void x86_check_arg_group (X86Fn *f, IRInstr *first, IRInstr *end)
{
	IRInstr *base = f->cg->ir->instrs + f->fn->start;
	IRInstr *a = first;
	uint32_t k = 0;

	while (a < end && a->op == IR_ARG) {
		f->blk = f->ctx.ins_block[a - base];
		if (a->target != k) {
			f->at = (uint32_t)(a - base);
			regalloc_violation(f, "ARG targets must ascend from 0 without gaps");
		}
		k++;
		a++;
	}
	if (a >= end || a->op != IR_CALL || a->argc != k) {
		f->at = (uint32_t)(first - base);
		regalloc_violation(f, "ARG group must end in CALL with argc == length");
	}
}
