#include "codegen/x86_64.h"
#include "ir/opts/common.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#define SYM_PREFIX "__r2_"
#define INIT_SYM "__r2.init"

#define RED_ZONE 128
#define X86_REG_NUM 14
#define X86_CALLEE_MASK 0x3E00ull
#define ARG_REGS_MASK 0x7Eull

static const char *asm_size_name[9] =
{
	[1] = ".byte",
	[2] = ".word",
	[4] = ".long",
	[8] = ".quad"
};

static void make_global_data (CodeGen *c, uint32_t i)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;

	cg_printf(c, "\t.balign %u\n" SYM_PREFIX "%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, "%s ", asm_size_name[size]);

	if (types[g->type].sign) cg_printf(c, "%lld\n", (long long)g->init);
	else cg_printf(c, "%llu\n", (unsigned long long)g->init);
}

static void make_global_bss (CodeGen *c, uint32_t i)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;
	cg_printf(c, "\t.balign %u\n" SYM_PREFIX "%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, ".zero %u\n", size);
}

static void make_globals (CodeGen *c)
{
	IR *ir = c->ir;
	int printed = 0;

	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (!ir->globals[i].has_init) continue;
		if (!printed) {
			printed = 1;
			cg_printf(c, "\t.section .data\n");
		}
		make_global_data(c, i);
	}
	printed = 0;
	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (ir->globals[i].has_init) continue;
		if (!printed) {
			printed = 1;
			cg_printf(c, "\t.section .bss\n");
		}
		make_global_bss(c, i);
	}
}

static int is_silent (X86Fn *f, IRInstr *i)
{
	if (i->op == IR_NOP) return 1;
	if (i->dst == NO_REG) return 0;
	if (f->regs[i->dst].cstate == VR_CONST || f->regs[i->dst].cstate == VR_ALIAS)
		return 1;
	return f->regs[i->dst].uses == 0 && is_deletable[i->op];
}

static IRInstr *next_live (X86Fn *f, IRInstr *i, IRInstr *end)
{
	i++;
	while (i < end && is_silent(f, i)) i++;
	return i;
}

static IRInstr *tail_ret (IRInstr *call);
static int tail_match (X86Fn *f, IRInstr *call, IRInstr *ret);

static uint32_t scan_outg (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	uint32_t out = 0;

	f->leaf = 1;
	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *instr = ir->instrs + fn->start + i;
		uint32_t n;

		if (instr->op != IR_CALL) continue;
		if (tail_match(f, instr, tail_ret(instr))) continue;
		f->leaf = 0;

		if (instr->argc <= 6) continue;
		n = (instr->argc - 6) << 3;
		if (n > out) out = n;
	}
	return out;
}

static uint32_t save_area (X86Fn *f, uint32_t c)
{
	uint64_t mask = f->ctx.callee_used;

	for (uint32_t i = 0; i < f->fn->count; i++)
		mask |= f->ctx.save[i];

	f->regsave = 0;
	f->savemask = mask;
	if (!mask) return c;
	f->regsave = (c + 7) & ~7u;
	return f->regsave + 8u * (uint32_t)__builtin_popcountll(mask);
}

static void fill_regalloc (X86Fn *f)
{
	IRFn *fn = f->fn;
	int spill_all = f->cg->opt_level == NO_OPT;

	f->ctx.ins_block = f->blocks;
	f->ctx.save = f->saves;
	f->ctx.free_regs = f->frees;
	f->ctx.callee_mask = X86_CALLEE_MASK;
	regalloc(f->cg->arena, f->cg->ir, fn, f->regs, &f->ctx,
			X86_REG_NUM, spill_all);
}

static uint32_t max_slot (X86Fn *f)
{
	uint32_t m = 0;

	for (uint32_t r = 0; r < f->fn->reg_count; r++)
		if (f->regs[r].slot != 0 && f->regs[r].slot > m)
			m = f->regs[r].slot;
	return m;
}

static void layout_fn (X86Fn *f)
{
	uint32_t c;

	fill_regalloc(f);
	c = save_area(f, max_slot(f));
	f->outgoing = scan_outg(f);
	c += f->outgoing;
	f->leaf = f->leaf && c <= RED_ZONE;
	f->frame = f->leaf ? 0 : (c + 15) & ~15u;
	f->base = f->leaf ? "%rsp" : "%rbp";
}

static void print_fn_name (CodeGen *c, IRFn *fn)
{
	if (fn == c->ir->fns + c->ir->init_fn) cg_printf(c, INIT_SYM);
	else cg_printf(c, SYM_PREFIX "%.*s", (int)fn->len, fn->name);
}

enum {
	RAX = 0,
	RCX = 1,
	RDX = 2
};

static const char *regs_names[] =
{
	// 1 byte
	"%al", "%cl", "%dl", "%sil",
	"%dil", "%r8b", "%r9b", "%r10b",
	"%r11b", "%bl", "%r12b", "%r13b",
	"%r14b", "%r15b", NULL, NULL,

	// 2 bytes
	"%ax", "%cx", "%dx", "%si",
	"%di", "%r8w", "%r9w", "%r10w",
	"%r11w", "%bx", "%r12w", "%r13w",
	"%r14w", "%r15w", NULL, NULL,

	// 4 bytes
	"%eax", "%ecx", "%edx", "%esi",
	"%edi", "%r8d", "%r9d", "%r10d",
	"%r11d", "%ebx", "%r12d", "%r13d",
	"%r14d", "%r15d", NULL, NULL,

	// 8 bytes
	"%rax", "%rcx", "%rdx", "%rsi",
	"%rdi", "%r8", "%r9", "%r10",
	"%r11", "%rbx", "%r12", "%r13",
	"%r14", "%r15", NULL, NULL
};

static const uint8_t size_idx[9] = { [1] = 0, [2] = 1, [4] = 2, [8] = 3 };
static const char mem_suf[9] = { [1] = 'b', [2] = 'w', [4] = 'l', [8] = 'q' };
static const uint8_t arg_regs[6] = { 4, 3, 2, 1, 5, 6 };

static inline const char *reg_name (uint8_t reg, uint8_t size)
{
	return regs_names[(size_idx[size] << 4) + reg];
}

static void move_saved (X86Fn *f, uint64_t mask, int restore)
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

static int phys_reg (X86Fn *f, uint32_t vreg)
{
	int8_t h = f->regs[vreg].home[f->blk];

	if (h < 0 || f->regs[vreg].cstate == VR_CONST) return -1;
	return h;
}

static uint64_t home_bit (X86Fn *f, uint32_t vreg)
{
	int p = vreg == NO_REG ? -1 : phys_reg(f, vreg);

	return p < 0 ? 0 : 1ull << p;
}

static void begin_instr (X86Fn *f, IRInstr *i)
{
	uint32_t *src[3];
	int n = get_srcs(i, src);

	f->at = (uint32_t)(i - (f->cg->ir->instrs + f->fn->start));
	f->blk = f->ctx.ins_block[f->at];
	f->pool = f->ctx.free_regs[f->at];
	f->busy = home_bit(f, i->dst);
	for (int k = 0; k < n; k++)
		f->busy |= home_bit(f, *src[k]);
}

static void regalloc_violation (X86Fn *f, const char *what)
{
	fprintf(stderr, "internal error: regalloc contract broken in '%.*s', instr %u: %s\n",
			(int)f->fn->len, f->fn->name, f->at, what);
	exit(1);
}

static uint8_t grab (X86Fn *f)
{
	uint64_t m = f->pool & ~f->busy;
	if (!m) regalloc_violation(f, "no scratch register available");

	uint8_t r = (uint8_t)__builtin_ctzll(m);
	f->pool &= ~(1ull << r);
	return r;
}

static void take_fixed (X86Fn *f, uint8_t r, uint32_t keeps)
{
	if (keeps != NO_REG && phys_reg(f, keeps) == r) return;
	if (!(f->pool >> r & 1)) regalloc_violation(f, "fixed register is not free");
	f->pool &= ~(1ull << r);
}

static uint8_t work_reg (X86Fn *f, uint32_t dst, uint64_t avoid)
{
	int d = phys_reg(f, dst);

	if (d >= 0 && !(avoid >> d & 1)) return (uint8_t)d;
	return grab(f);
}

static const char *const load_ops[8] = {
	"movzbl", "movsbq", "movzwl", "movswq",
	"movl", "movslq", "movq", "movq"
};

static const char *load_op (uint8_t t, uint8_t *size)
{
	uint8_t s = types[t].size;
	uint8_t sign = types[t].sign;

	*size = (s < 8 && !sign) ? 4 : 8;
	return load_ops[(size_idx[s] << 1) | sign];
}

static int64_t const_ext (X86Fn *f, uint32_t vreg)
{
	const Type *t = types + f->fn->reg_types[vreg];
	unsigned sh = 64u - t->size * 8u;
	uint64_t v = (uint64_t)f->regs[vreg].cval << sh;

	return t->sign ? (int64_t)v >> sh : (int64_t)(v >> sh);
}

static void load_const (X86Fn *f, uint8_t reg, int64_t v)
{
	CodeGen *c = f->cg;

	if (v >= 0 && v <= UINT32_MAX)
		cg_printf(c, "\tmovl $%lld, %s\n", (long long)v, reg_name(reg, 4));
	else if (v == (int32_t)v)
		cg_printf(c, "\tmovq $%lld, %s\n", (long long)v, reg_name(reg, 8));
	else cg_printf(c, "\tmovabsq $0x%llx, %s\n",
			(unsigned long long)v, reg_name(reg, 8));
}

static void load_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	int p = phys_reg(f, vreg);

	if (f->regs[vreg].cstate == VR_CONST) {
		load_const(f, reg, const_ext(f, vreg));
		return;
	}
	if (p >= 0) {
		if (p != reg)
			cg_printf(f->cg, "\tmovq %s, %s\n", reg_name(p, 8), reg_name(reg, 8));
		return;
	}

	uint8_t w;
	const char *op = load_op(f->fn->reg_types[vreg], &w);
	cg_printf(f->cg, "\t%s -%u(%s), %s\n",
			op, f->regs[vreg].slot, f->base, reg_name(reg, w));
}

static void store_phys (X86Fn *f, uint8_t from, uint8_t to, uint8_t type)
{
	uint8_t s = types[type].size;
	if (from == to && s == 8) return;

	uint8_t w;
	const char *op = load_op(type, &w);
	cg_printf(f->cg, "\t%s %s, %s\n", op, reg_name(from, s), reg_name(to, w));
}

static void store_reg (X86Fn *f, uint8_t reg, uint32_t vreg)
{
	uint8_t t = f->fn->reg_types[vreg];
	uint8_t s = types[t].size;
	int p = phys_reg(f, vreg);

	if (!f->regs[vreg].uses) return;
	if (p >= 0) {
		store_phys(f, reg, (uint8_t)p, t);
		return;
	}
	cg_printf(f->cg, "\tmov%c %s, -%u(%s)\n",
			mem_suf[s], reg_name(reg, s), f->regs[vreg].slot, f->base);
}

static void store_imm (X86Fn *f, uint32_t vreg, int64_t v)
{
	CodeGen *c = f->cg;
	uint8_t t = f->fn->reg_types[vreg];
	uint8_t s = types[t].size;
	int p = phys_reg(f, vreg);
	uint8_t w;

	if (p >= 0) {
		load_const(f, (uint8_t)p, norm_val((uint64_t)v, t));
		return;
	}
	if (s == 8 && v != (int32_t)v) {
		w = grab(f);
		cg_printf(c, "\tmovabsq $0x%llx, %s\n", (unsigned long long)v, reg_name(w, 8));
		store_reg(f, w, vreg);
		return;
	}
	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;

	cg_printf(c, "\tmov%c $%lld, -%u(%s)\n",
			mem_suf[s], (long long)v, f->regs[vreg].slot, f->base);
}

static void check_homes (X86Fn *f)
{
	for (uint32_t b = 0; b < f->ctx.block_count; b++) {
		for (uint32_t r = 0; r < f->fn->reg_count; r++) {
			int h = f->regs[r].home[b];

			if (h < VR_SPILL || h >= X86_REG_NUM)
				regalloc_violation(f, "vreg home out of range");
			else if (h >= 0 && (X86_CALLEE_MASK >> h & 1)
					&& !(f->ctx.callee_used >> h & 1))
				regalloc_violation(f, "callee-saved home missing from callee_used");
		}
	}
}

static void check_ctx (X86Fn *f)
{
	IRInstr *code = f->cg->ir->instrs + f->fn->start;

	for (uint32_t i = 0; i < f->fn->count; i++) {
		f->at = i;
		f->blk = f->ctx.ins_block[i];
		if (f->frees[i] & X86_CALLEE_MASK & ~f->ctx.callee_used)
			regalloc_violation(f, "free_regs lists a callee-saved reg not in callee_used");
		if (code[i].op == IR_CALL && (f->saves[i] & home_bit(f, code[i].dst)))
			regalloc_violation(f, "save[] contains the home of the call's dst");
	}
}

static int param_clobbers (X86Fn *f, IRInstr *code, uint32_t j, uint32_t n)
{
	int h = phys_reg(f, code[j].dst);

	for (uint32_t m = j + 1; m < n; m++)
		if (h == arg_regs[code[m].target] && f->regs[code[m].dst].uses) return 1;
	return 0;
}

static void check_params (X86Fn *f)
{
	IRInstr *code = f->cg->ir->instrs + f->fn->start;
	uint32_t n = f->fn->param_count < 6 ? f->fn->param_count : 6;

	for (uint32_t j = 0; j < n && code[j].op == IR_PARAM; j++) {
		f->blk = f->ctx.ins_block[j];
		if (!param_clobbers(f, code, j, n)) continue;
		f->at = j;
		regalloc_violation(f, "PARAM overwrites a later incoming argument");
	}
}

static void check_regalloc (X86Fn *f)
{
	f->at = 0;
	f->blk = 0;
	check_homes(f);
	check_ctx(f);
	check_params(f);
}

static int arg_clobbered (X86Fn *f, IRInstr *first, IRInstr *a)
{
	int h = phys_reg(f, a->src1);

	for (IRInstr *b = first; b < a && b->target < 6; b++)
		if (h == arg_regs[b->target] && b->src1 != a->src1) return 1;
	return 0;
}

static void check_arg_group (X86Fn *f, IRInstr *first, IRInstr *end)
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
		if (arg_clobbered(f, first, a)) {
			f->at = (uint32_t)(a - base);
			regalloc_violation(f, "ARG source clobbered by an earlier ARG of its group");
		}
		k++;
		a++;
	}
	if (a >= end || a->op != IR_CALL || a->argc != k) {
		f->at = (uint32_t)(first - base);
		regalloc_violation(f, "ARG group must end in CALL with argc == length");
	}
}

static void make_param (X86Fn *f, IRInstr *i)
{
	if (!f->regs[i->dst].uses) return;
	if (i->target < 6) {
		store_reg(f, arg_regs[i->target], i->dst);
		return;
	}
	uint8_t w = work_reg(f, i->dst, 0);
	cg_printf(f->cg, "\tmovq %u(%s), %s\n",
			(f->leaf ? 8 : 16) + ((i->target - 6) << 3), f->base, reg_name(w, 8));
	store_reg(f, w, i->dst);
}

static void make_move (X86Fn *f, IRInstr *i)
{
	if (i->dst == i->src1) return;
	if (f->regs[i->src1].cstate == VR_CONST) {
		store_imm(f, i->dst, const_ext(f, i->src1));
		return;
	}
	int p = phys_reg(f, i->src1);
	int pd = phys_reg(f, i->dst);

	if (p >= 0 && pd >= 0) {
		if (!f->regs[i->dst].uses) return;
		if (same_rep(f->fn, i->dst, i->src1)) {
			if (p == pd) return;
			cg_printf(f->cg, "\tmovq %s, %s\n",
					reg_name((uint8_t)p, 8), reg_name((uint8_t)pd, 8));
			return;
		}
		store_reg(f, (uint8_t)p, i->dst);
		return;
	}
	if (p >= 0) {
		store_reg(f, (uint8_t)p, i->dst);
		return;
	}
	uint8_t w = work_reg(f, i->dst, 0);
	load_reg(f, w, i->src1);
	store_reg(f, w, i->dst);
}

static void make_ld_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;
	int p = phys_reg(f, i->dst);
	uint8_t w = p >= 0 ? (uint8_t)p : grab(f);
	uint8_t size;
	const char *op = load_op(g->type, &size);

	cg_printf(c, "\t%s " SYM_PREFIX "%.*s(%%rip), %s\n", op,
			(int)g->len, g->name, reg_name(w, size));
	if (p < 0) store_reg(f, w, i->dst);
}

static void make_str_global (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRGlobal *g = c->ir->globals + i->target;
	uint8_t s = types[g->type].size;
	int p = phys_reg(f, i->src1);

	if (f->regs[i->src1].cstate == VR_CONST) {
		int64_t v = const_ext(f, i->src1);

		if (s == 8 && v != (int32_t)v) {
			uint8_t w = grab(f);
			cg_printf(c, "\tmovabsq $0x%llx, %s\n",
					(unsigned long long)v, reg_name(w, 8));
			cg_printf(c, "\tmovq %s, " SYM_PREFIX "%.*s(%%rip)\n",
					reg_name(w, 8), (int)g->len, g->name);
			return;
		}
		if (s == 4) v = (int32_t)v;
		else if (s == 2) v = (int16_t)v;
		else if (s == 1) v = (int8_t)v;
		cg_printf(c, "\tmov%c $%lld, " SYM_PREFIX "%.*s(%%rip)\n", mem_suf[s],
				(long long)v, (int)g->len, g->name);
		return;
	}
	if (p < 0) {
		p = grab(f);
		load_reg(f, (uint8_t)p, i->src1);
	}
	cg_printf(c, "\tmov%c %s, " SYM_PREFIX "%.*s(%%rip)\n", mem_suf[s],
			reg_name(p, s), (int)g->len, g->name);
}

static void op_reg (X86Fn *f, const char *op, uint8_t w, uint32_t vreg)
{
	int is_const = f->regs[vreg].cstate == VR_CONST;
	int64_t v = is_const ? const_ext(f, vreg) : 0;
	int p = phys_reg(f, vreg);
	uint64_t keep = f->pool;

	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\t%sq $%lld, %s\n", op, (long long)v, reg_name(w, 8));
	} else if (p >= 0) {
		cg_printf(f->cg, "\t%sq %s, %s\n", op, reg_name(p, 8), reg_name(w, 8));
	} else if (!is_const && types[f->fn->reg_types[vreg]].size == 8) {
		cg_printf(f->cg, "\t%sq -%u(%s), %s\n", op,
				f->regs[vreg].slot, f->base, reg_name(w, 8));
	} else {
		uint8_t t = grab(f);
		load_reg(f, t, vreg);
		cg_printf(f->cg, "\t%sq %s, %s\n", op, reg_name(t, 8), reg_name(w, 8));
	}
	f->pool = keep;
}

static const char *arith_ops[IR_COUNT] = {
	[IR_ADD] = "add",
	[IR_SUB] = "sub",
	[IR_MUL] = "imul",
	[IR_AND_A] = "and",
	[IR_OR_A] = "or",
	[IR_XOR] = "xor"
};

static uint64_t shift_amt (uint64_t v)
{
	uint64_t k = 0;

	while ((v & 1u) == 0) {
		v >>= 1;
		k++;
	}
	return k;
}

static void make_mul_pm1 (X86Fn *f, uint8_t w, uint64_t k, int plus)
{
	CodeGen *c = f->cg;
	uint8_t t = grab(f);
	const char *wn = reg_name(w, 8);
	const char *tn = reg_name(t, 8);

	cg_printf(c, "\tmovq %s, %s\n\tshlq $%llu, %s\n", wn, tn, (unsigned long long)k, tn);
	if (plus) cg_printf(c, "\taddq %s, %s\n", tn, wn);
	else cg_printf(c, "\tsubq %s, %s\n\tmovq %s, %s\n", wn, tn, tn, wn);
}

static int make_mul_const (X86Fn *f, IRInstr *i)
{
	if (f->regs[i->src2].cstate != VR_CONST) return 0;
	int64_t v = const_ext(f, i->src2);
	uint8_t w = work_reg(f, i->dst, 0);
	const char *wn = reg_name(w, 8);
	load_reg(f, w, i->src1);

	if (v > 0 && (v & (v - 1)) == 0)
		cg_printf(f->cg, "\tshlq $%llu, %s\n",
				(unsigned long long)shift_amt((uint64_t)v), wn);
	else if (v == 3 || v == 5 || v == 9)
		cg_printf(f->cg, "\tleaq (%s,%s,%lld), %s\n", wn, wn, (long long)(v - 1), wn);
	else if (v > 9 && !((uint64_t)(v - 1) & (uint64_t)(v - 2)))
		make_mul_pm1(f, w, shift_amt((uint64_t)(v - 1)), 1);
	else if (v > 3 && !((uint64_t)v & ((uint64_t)v + 1)))
		make_mul_pm1(f, w, shift_amt((uint64_t)v + 1), 0);
	else op_reg(f, "imul", w, i->src2);

	store_reg(f, w, i->dst);
	return 1;
}

static void make_arith (X86Fn *f, IRInstr *i)
{
	if (i->op == IR_MUL && make_mul_const(f, i)) return;
	if (i->op == IR_ADD && types[i->data_type].size == 8) {
		int pa = phys_reg(f, i->src1);
		int pb = phys_reg(f, i->src2);
		int pd = phys_reg(f, i->dst);

		if (pa >= 0 && pb >= 0 && pd >= 0 && pd != pa && pd != pb) {
			cg_printf(f->cg, "\tleaq (%s,%s), %s\n",
					reg_name((uint8_t)pa, 8), reg_name((uint8_t)pb, 8),
					reg_name((uint8_t)pd, 8));
			return;
		}
	}
	uint8_t w = work_reg(f, i->dst, home_bit(f, i->src2));

	load_reg(f, w, i->src1);
	op_reg(f, arith_ops[i->op], w, i->src2);
	store_reg(f, w, i->dst);
}

static const char *sext_ops[9] = {
	[1] = "cbtw",
	[2] = "cwtd",
	[4] = "cltd",
	[8] = "cqto"
};

static uint64_t magic_unsigned (uint64_t d, unsigned *l)
{
	unsigned k = 64 - (unsigned)__builtin_clzll(d - 1);
	uint64_t low = k == 64 ? 0 - d : (1ull << k) - d;

	*l = k;
	return (uint64_t)(((unsigned __int128)low << 64) / d) + 1;
}

static uint64_t magic_signed (uint64_t d, unsigned *sh)
{
	uint64_t two63 = 1ull << 63;
	uint64_t anc = two63 - 1 - two63 % d;

	uint64_t q1 = two63 / anc;
	uint64_t r1 = two63 - q1 * anc;
	uint64_t q2 = two63 / d;
	uint64_t r2 = two63 - q2 * d;

	uint64_t delta;
	unsigned p = 63;

	do {
		p++;
		q1 <<= 1;
		r1 <<= 1;
		if (r1 >= anc) { q1++; r1 -= anc; }
		q2 <<= 1;
		r2 <<= 1;
		if (r2 >= d) { q2++; r2 -= d; }
		delta = d - r2;
	} while (q1 < delta || (q1 == delta && r1 == 0));

	*sh = p - 64;
	return q2 + 1;
}

static void make_mod_tail (X86Fn *f, uint8_t t, uint64_t d)
{
	CodeGen *c = f->cg;

	if (d <= INT32_MAX)
		cg_printf(c, "\timulq $%llu, %%rax, %%rax\n", (unsigned long long)d);
	else cg_printf(c, "\tmovabsq $0x%llx, %%rdx\n\timulq %%rdx, %%rax\n",
			(unsigned long long)d);
	cg_printf(c, "\tsubq %%rax, %s\n\tmovq %s, %%rax\n", reg_name(t, 8), reg_name(t, 8));
}

static void make_udiv_magic (X86Fn *f, uint8_t t, int is_div, uint64_t d)
{
	CodeGen *c = f->cg;
	const char *tn = reg_name(t, 8);
	unsigned l;
	uint64_t m = magic_unsigned(d, &l);

	cg_printf(c, "\tmovq %%rax, %s\n\tmovabsq $0x%llx, %%rdx\n\tmulq %%rdx\n",
			tn, (unsigned long long)m);
	cg_printf(c, "\tmovq %s, %%rax\n\tsubq %%rdx, %%rax\n\tshrq $1, %%rax\n", tn);
	cg_printf(c, "\taddq %%rdx, %%rax\n\tshrq $%u, %%rax\n", l - 1);
	if (!is_div) make_mod_tail(f, t, d);
}

static void make_sdiv_magic (X86Fn *f, uint8_t t, int is_div, uint64_t d)
{
	CodeGen *c = f->cg;
	const char *tn = reg_name(t, 8);
	unsigned s;
	uint64_t m = magic_signed(d, &s);

	cg_printf(c, "\tmovq %%rax, %s\n\tmovabsq $0x%llx, %%rdx\n\timulq %%rdx\n",
			tn, (unsigned long long)m);
	if ((int64_t)m < 0) cg_printf(c, "\taddq %s, %%rdx\n", tn);
	if (s) cg_printf(c, "\tsarq $%u, %%rdx\n", s);
	cg_printf(c, "\tmovq %%rdx, %%rax\n\tshrq $63, %%rax\n\taddq %%rdx, %%rax\n");
	if (!is_div) make_mod_tail(f, t, d);
}

static void make_div_pow2 (X86Fn *f, int sign, int is_div, int64_t v)
{
	CodeGen *c = f->cg;
	unsigned long long k = shift_amt((uint64_t)v);

	if (!sign) {
		if (is_div) cg_printf(c, "\tshrq $%llu, %%rax\n", k);
		else cg_printf(c, "\tandq $%lld, %%rax\n", (long long)(v - 1));
		return;
	}
	cg_printf(c, "\tmovq %%rax, %%rdx\n\tsarq $63, %%rdx\n");
	cg_printf(c, "\tshrq $%llu, %%rdx\n\taddq %%rdx, %%rax\n", 64 - k);

	if (is_div) cg_printf(c, "\tsarq $%llu, %%rax\n", k);
	else cg_printf(c, "\tandq $%lld, %%rax\n\tsubq %%rdx, %%rax\n", (long long)(v - 1));
}

static int make_divmod_const (X86Fn *f, IRInstr *i)
{
	int sign = types[i->data_type].sign;
	int is_div = i->op == IR_DIV;

	if (f->regs[i->src2].cstate != VR_CONST) return 0;
	int64_t v = const_ext(f, i->src2);
	if (v <= 0 || (sign && v == 1)) return 0;
	int pow2 = (v & (v - 1)) == 0;
	if (pow2 && !is_div && v - 1 != (int32_t)(v - 1)) return 0;

	load_reg(f, RAX, i->src1);
	if (pow2) make_div_pow2(f, sign, is_div, v);
	else if (sign) make_sdiv_magic(f, grab(f), is_div, (uint64_t)v);
	else make_udiv_magic(f, grab(f), is_div, (uint64_t)v);

	store_reg(f, RAX, i->dst);
	return 1;
}

static void make_divmod (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	uint8_t s = types[i->data_type].size;
	int sign = types[i->data_type].sign;

	take_fixed(f, RAX, i->src1);
	take_fixed(f, RDX, i->src2);
	if (make_divmod_const(f, i)) return;

	int p = phys_reg(f, i->src2);
	if (p < 0 || p == RAX || p == RDX) {
		p = grab(f);
		load_reg(f, (uint8_t)p, i->src2);
	}
	load_reg(f, RAX, i->src1);

	if (sign) cg_printf(c, "\t%s\n", sext_ops[s]);
	else if (s > 1) cg_printf(c, "\txorl %%edx, %%edx\n");
	cg_printf(c, "\t%s%c %s\n", sign ? "idiv" : "div", mem_suf[s], reg_name(p, s));

	if (i->op == IR_DIV) store_reg(f, RAX, i->dst);
	else if (s > 1) store_reg(f, RDX, i->dst);
	else if (f->regs[i->dst].uses) {
		cg_printf(c, "\tmovzbl %%ah, %%eax\n");
		store_reg(f, RAX, i->dst);
	}
}

static void make_shift (X86Fn *f, IRInstr *i)
{
	const char *op = "shl";
	int imm = f->regs[i->src2].cstate == VR_CONST;
	int64_t v = imm ? const_ext(f, i->src2) & 63 : 0;
	uint64_t avoid = 0;

	if (i->op == IR_RS) op = types[i->data_type].sign ? "sar" : "shr";
	if (!imm) {
		take_fixed(f, RCX, i->src2);
		avoid = home_bit(f, i->src2) | (1ull << RCX);
	}

	uint8_t w = work_reg(f, i->dst, avoid);
	load_reg(f, w, i->src1);

	if (imm) {
		cg_printf(f->cg, "\t%sq $%lld, %s\n", op, (long long)v, reg_name(w, 8));
	} else {
		load_reg(f, RCX, i->src2);
		cg_printf(f->cg, "\t%sq %%cl, %s\n", op, reg_name(w, 8));
	}
	store_reg(f, w, i->dst);
}

static void make_unary (X86Fn *f, IRInstr *i, const char *op)
{
	uint8_t w = work_reg(f, i->dst, 0);

	load_reg(f, w, i->src1);
	cg_printf(f->cg, "\t%sq %s\n", op, reg_name(w, 8));
	store_reg(f, w, i->dst);
}

static void make_not_l (X86Fn *f, IRInstr *i)
{
	uint8_t w = work_reg(f, i->dst, 0);
	const char *q = reg_name(w, 8);
	const char *b = reg_name(w, 1);

	load_reg(f, w, i->src1);
	cg_printf(f->cg, "\ttestq %s, %s\n\tsete %s\n\tmovzbl %s, %s\n",
			q, q, b, b, reg_name(w, 4));
	store_reg(f, w, i->dst);
}

static const char *comp_names[12] = {
	"e", "ne", "g", "ge", "l", "le",
	"e", "ne", "a", "ae", "b", "be"
};

static int cmp_mem_imm (X86Fn *f, IRInstr *i)
{
	uint32_t a = i->src1;
	uint8_t s = types[f->fn->reg_types[a]].size;

	if (f->regs[i->src2].cstate != VR_CONST) return 0;
	if (f->regs[a].cstate != VR_MEM && f->regs[a].cstate != VR_ALIAS) return 0;
	if (phys_reg(f, a) >= 0) return 0;

	int64_t v = const_ext(f, i->src2);
	if (s == 8 && v != (int32_t)v) return 0;
	if (s == 4) v = (int32_t)v;
	else if (s == 2) v = (int16_t)v;
	else if (s == 1) v = (int8_t)v;

	cg_printf(f->cg, "\tcmp%c $%lld, -%u(%s)\n", mem_suf[s],
			(long long)v, f->regs[a].slot, f->base);
	return 1;
}

static void write_cmp (X86Fn *f, IRInstr *i)
{
	uint64_t keep = f->pool;
	if (cmp_mem_imm(f, i)) return;

	int p = phys_reg(f, i->src1);
	if (p < 0) {
		p = grab(f);
		load_reg(f, (uint8_t)p, i->src1);
	}
	op_reg(f, "cmp", (uint8_t)p, i->src2);
	f->pool = keep;
}

static void make_cmp (X86Fn *f, IRInstr *i)
{
	int base = types[i->data_type].sign ? 0 : 6;
	const char *cc = comp_names[base + (i->op - IR_EQ)];
	uint8_t w;

	write_cmp(f, i);
	w = work_reg(f, i->dst, 0);
	cg_printf(f->cg, "\tset%s %s\n\tmovzbl %s, %s\n",
			cc, reg_name(w, 1), reg_name(w, 1), reg_name(w, 4));
	store_reg(f, w, i->dst);
}

static const uint8_t inv_cmp[6] = { 1, 0, 5, 4, 3, 2 };

static int fuses_with_next (X86Fn *f, IRInstr *i)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *n = next_live(f, i, end);

	if (n == end) return 0;
	if (n->op != IR_JZ && n->op != IR_JNZ) return 0;
	return n->src1 == i->dst && f->regs[i->dst].uses == 1;
}

static void make_cmp_jump (X86Fn *f, IRInstr *cmp, IRInstr *jmp)
{
	int base = types[cmp->data_type].sign ? 0 : 6;
	int cc = cmp->op - IR_EQ;

	if (jmp->op == IR_JZ) cc = inv_cmp[cc];
	write_cmp(f, cmp);
	cg_printf(f->cg, "\tj%s .L%u\n", comp_names[base + cc], jmp->target);
}

static void select_move (X86Fn *f, IRInstr *sel, const char *yes, const char *no)
{
	int pa = phys_reg(f, sel->src1);
	int pb = phys_reg(f, sel->src2);
	int pd = phys_reg(f, sel->dst);
	uint8_t w, r;

	if (pd >= 0 && pd == pb) {
		r = pa >= 0 ? (uint8_t)pa : grab(f);
		if (pa < 0) load_reg(f, r, sel->src1);
		cg_printf(f->cg, "\tcmov%s %s, %s\n", yes, reg_name(r, 8), reg_name(pd, 8));
		return;
	}
	w = work_reg(f, sel->dst, 0);
	load_reg(f, w, sel->src1);
	r = pb >= 0 ? (uint8_t)pb : grab(f);
	if (pb < 0) load_reg(f, r, sel->src2);
	cg_printf(f->cg, "\tcmov%s %s, %s\n", no, reg_name(r, 8), reg_name(w, 8));
	store_reg(f, w, sel->dst);
}

static void make_cmp_select (X86Fn *f, IRInstr *cmp, IRInstr *sel)
{
	int base = types[cmp->data_type].sign ? 0 : 6;
	int cc = cmp->op - IR_EQ;

	f->busy |= home_bit(f, sel->src1) | home_bit(f, sel->src2) | home_bit(f, sel->dst);
	write_cmp(f, cmp);
	select_move(f, sel, comp_names[base + cc], comp_names[base + inv_cmp[cc]]);
}

static int make_cmp_instr (X86Fn *f, IRInstr *i)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *n = next_live(f, i, end);

	if (n != end && fuses_with_next(f, i)) {
		make_cmp_jump(f, i, n);
		return (int)(n - i) + 1;
	}
	if (n != end && n->op == IR_SELECT && n->cond == i->dst
			&& f->regs[i->dst].uses == 1) {
		make_cmp_select(f, i, n);
		return (int)(n - i) + 1;
	}
	make_cmp(f, i);
	return 1;
}

static void test_zero (X86Fn *f, uint32_t vreg)
{
	int p = phys_reg(f, vreg);
	uint8_t s = types[f->fn->reg_types[vreg]].size;
	uint64_t keep = f->pool;
	uint8_t t;

	if (f->regs[vreg].cstate == VR_CONST) {
		t = grab(f);
		load_reg(f, t, vreg);
		cg_printf(f->cg, "\ttestq %s, %s\n", reg_name(t, 8), reg_name(t, 8));
		f->pool = keep;
		return;
	}
	if (p >= 0)
		cg_printf(f->cg, "\ttestq %s, %s\n", reg_name(p, 8), reg_name(p, 8));
	else cg_printf(f->cg, "\tcmp%c $0, -%u(%s)\n", mem_suf[s],
			f->regs[vreg].slot, f->base);
}

static void make_jump (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;

	if (i->op == IR_LABEL) {
		cg_printf(c, ".L%u:\n", i->target);
		return;
	}
	if (i->op == IR_JMP) {
		cg_printf(c, "\tjmp .L%u\n", i->target);
		return;
	}
	if (f->regs[i->src1].cstate == VR_CONST) {
		int taken = (const_ext(f, i->src1) != 0) == (i->op == IR_JNZ);
		if (taken) cg_printf(c, "\tjmp .L%u\n", i->target);
		return;
	}
	test_zero(f, i->src1);
	cg_printf(c, "\t%s .L%u\n", i->op == IR_JZ ? "je" : "jne", i->target);
}

static IRInstr *group_call (IRInstr *arg, IRInstr *end)
{
	while (arg < end && arg->op == IR_ARG) arg++;
	return arg;
}

static void move_call_live (X86Fn *f, IRInstr *call, int restore)
{
	uint32_t at = (uint32_t)(call - (f->cg->ir->instrs + f->fn->start));
	uint64_t keep = f->ctx.save[at] & ~X86_CALLEE_MASK;

	move_saved(f, keep, restore);
}

static void make_arg (X86Fn *f, IRInstr *i)
{
	int is_const = f->regs[i->src1].cstate == VR_CONST;
	int64_t v = is_const ? const_ext(f, i->src1) : 0;
	int p = phys_reg(f, i->src1);

	if (i->target == 0) {
		IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
		IRInstr *call = group_call(i, end);

		check_arg_group(f, i, end);
		if (!tail_match(f, call, tail_ret(call)))
			move_call_live(f, call, 0);
	}
	if (i->target < 6) {
		take_fixed(f, arg_regs[i->target], i->src1);
		load_reg(f, arg_regs[i->target], i->src1);
		return;
	}
	if (is_const && v == (int32_t)v) {
		cg_printf(f->cg, "\tmovq $%lld, %u(%%rsp)\n",
				(long long)v, (i->target - 6) << 3);
		return;
	}

	uint8_t w = 0;
	f->busy |= ARG_REGS_MASK;
	w = p >= 0 ? (uint8_t)p : grab(f);
	if (p < 0) load_reg(f, w, i->src1);
	cg_printf(f->cg, "\tmovq %s, %u(%%rsp)\n", reg_name(w, 8), (i->target - 6) << 3);
}

static void make_call (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;

	if (i->argc == 0) move_call_live(f, i, 0);
	cg_printf(c, "\tcall ");
	print_fn_name(c, c->ir->fns + i->target);
	cg_printf(c, "\n");

	if (i->dst != NO_REG)
		store_reg(f, RAX, i->dst);
	move_call_live(f, i, 1);
}

static void make_ret (X86Fn *f, IRInstr *i)
{
	if (i->src1 != NO_REG) {
		take_fixed(f, RAX, i->src1);
		load_reg(f, RAX, i->src1);
	}
	move_saved(f, f->ctx.callee_used, 1);
	cg_printf(f->cg, f->leaf ? "\tret\n" : "\tleave\n\tret\n");
}

enum { RMW_NONE = 0, RMW_UNARY, RMW_ARITH, RMW_SHIFT };

static const uint8_t rmw_kind[IR_COUNT] = {
	[IR_ADD] = RMW_ARITH, [IR_SUB] = RMW_ARITH, [IR_AND_A] = RMW_ARITH,
	[IR_OR_A] = RMW_ARITH, [IR_XOR] = RMW_ARITH,
	[IR_NEG] = RMW_UNARY, [IR_NOT_A] = RMW_UNARY,
	[IR_MUL] = RMW_SHIFT, [IR_LS] = RMW_SHIFT, [IR_RS] = RMW_SHIFT
};

static const char *const rmw_names[IR_COUNT] = {
	[IR_ADD] = "add", [IR_SUB] = "sub", [IR_AND_A] = "and",
	[IR_OR_A] = "or", [IR_XOR] = "xor", [IR_NEG] = "neg", [IR_NOT_A] = "not"
};

static int rmw_shift (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	int64_t k;

	if (f->regs[b->src2].cstate != VR_CONST) return 0;
	k = const_ext(f, b->src2);
	if (b->op == IR_MUL) {
		if (k <= 1 || (k & (k - 1))) return 0;
		k = (int64_t)shift_amt((uint64_t)k);
	}
	if (k < 1 || k >= 8 * s) return 0;
	if (b->op == IR_RS) *op = types[b->data_type].sign ? "sar" : "shr";
	else *op = "shl";
	*v = k;
	return 1;
}

static int rmw_arith (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	int64_t k;

	if (f->regs[b->src2].cstate != VR_CONST) return 0;
	k = const_ext(f, b->src2);
	if (s == 8 && k != (int32_t)k) return 0;
	if (s == 4) k = (int32_t)k;
	else if (s == 2) k = (int16_t)k;
	else if (s == 1) k = (int8_t)k;
	*op = rmw_names[b->op];
	*v = k;
	return 1;
}

static int rmw_operand (X86Fn *f, IRInstr *b, uint8_t s, const char **op, int64_t *v)
{
	switch (rmw_kind[b->op])
	{
	case RMW_UNARY: *op = rmw_names[b->op]; return 1;
	case RMW_ARITH: return rmw_arith(f, b, s, op, v);
	case RMW_SHIFT: return rmw_shift(f, b, s, op, v);
	default: return 0;
	}
}

static int rmw_reg_op (X86Fn *f, IRInstr *b, uint8_t s, const char **op)
{
	uint32_t y = b->src2;

	if (rmw_kind[b->op] != RMW_ARITH) return 0;
	if (y == NO_REG) return 0;
	if (phys_reg(f, y) < 0) return 0;

	if (types[f->fn->reg_types[y]].size != s) return 0;
	*op = rmw_names[b->op];
	return 1;
}

static int rmw_closes (IRInstr *a, IRInstr *c)
{
	if (a->op == IR_LD_GLOBAL)
		return c->op == IR_STR_GLOBAL && c->target == a->target;
	return c->op == IR_MOVE && c->dst == a->src1;
}

static IRInstr *rmw_find (X86Fn *f, IRInstr *a, IRInstr **mid)
{
	IRInstr *end = f->cg->ir->instrs + f->fn->start + f->fn->count;
	IRInstr *b = next_live(f, a, end);

	if (b == end || !rmw_kind[b->op] || b->src1 != a->dst) return NULL;
	IRInstr *c = next_live(f, b, end);
	if (c == end || c->src1 != b->dst || !rmw_closes(a, c)) return NULL;

	*mid = b;
	return c;
}

static uint8_t rmw_width (X86Fn *f, IRInstr *a)
{
	if (a->op == IR_LD_GLOBAL)
		return types[f->cg->ir->globals[a->target].type].size;
	return types[f->fn->reg_types[a->src1]].size;
}

static int rmw_shape_ok (X86Fn *f, IRInstr *a, IRInstr *b)
{
	uint8_t *rt = f->fn->reg_types;
	uint8_t s = rmw_width(f, a);

	if (f->regs[a->dst].uses != 1 || f->regs[b->dst].uses != 1) return 0;
	if (a->op != IR_LD_GLOBAL && (f->regs[a->src1].cstate != VR_MEM
				|| f->regs[a->src1].home[f->blk] != VR_SPILL)) return 0;
	return types[rt[a->dst]].size == s && types[rt[b->dst]].size == s;
}

static void print_rmw_loc (X86Fn *f, IRInstr *a)
{
	IRGlobal *g;

	if (a->op != IR_LD_GLOBAL) {
		cg_printf(f->cg, "-%u(%s)\n", f->regs[a->src1].slot, f->base);
		return;
	}
	g = f->cg->ir->globals + a->target;
	cg_printf(f->cg, SYM_PREFIX "%.*s(%%rip)\n", (int)g->len, g->name);
}

static int make_rmw (X86Fn *f, IRInstr *a)
{
	IRInstr *b = NULL;
	IRInstr *c = rmw_find(f, a, &b);
	const char *op = NULL;
	int64_t v = 0;

	if (!c || !rmw_shape_ok(f, a, b)) return 0;
	uint8_t s = rmw_width(f, a);

	if (rmw_operand(f, b, s, &op, &v)) {
		cg_printf(f->cg, "\t%s%c ", op, mem_suf[s]);
		if (rmw_kind[b->op] != RMW_UNARY)
			cg_printf(f->cg, "$%lld, ", (long long)v);
		print_rmw_loc(f, a);
		return (int)(c - a) + 1;
	}

	if (!rmw_reg_op(f, b, s, &op)) return 0;
	uint8_t r = (uint8_t)phys_reg(f, b->src2);
	cg_printf(f->cg, "\t%s%c %s, ", op, mem_suf[s], reg_name(r, s));
	print_rmw_loc(f, a);

	return (int)(c - a) + 1;
}

static int try_rmw_inplace (X86Fn *f, IRInstr *i)
{
	const char *op = NULL;
	int64_t v = 0;
	uint32_t x = i->dst;

	if (!rmw_kind[i->op] || i->dst != i->src1 || f->regs[x].cstate != VR_MEM
			|| f->regs[x].home[f->blk] != VR_SPILL)
		return 0;
	uint8_t s = types[f->fn->reg_types[x]].size;

	if (rmw_operand(f, i, s, &op, &v)) {
		cg_printf(f->cg, "\t%s%c ", op, mem_suf[s]);
		if (rmw_kind[i->op] != RMW_UNARY)
			cg_printf(f->cg, "$%lld, ", (long long)v);
		cg_printf(f->cg, "-%u(%s)\n", f->regs[x].slot, f->base);
		return 1;
	}

	if (!rmw_reg_op(f, i, s, &op)) return 0;
	uint8_t r = (uint8_t)phys_reg(f, i->src2);
	cg_printf(f->cg, "\t%s%c %s, -%u(%s)\n", op, mem_suf[s],
			reg_name(r, s), f->regs[x].slot, f->base);

	return 1;
}

static IRInstr *tail_ret (IRInstr *call)
{
	IRInstr *n = call + 1;

	while (n->op == IR_LABEL) n++;
	return n;
}

static int tail_match (X86Fn *f, IRInstr *call, IRInstr *ret)
{
	uint8_t from = f->cg->ir->fns[call->target].ret_type;
	uint8_t to = f->fn->ret_type;

	if (ret->op != IR_RET || call->argc > 6) return 0;
	if (ret->src1 != call->dst) return 0;
	if (call->dst != NO_REG && f->regs[call->dst].uses != 1) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

static int make_tail_call (X86Fn *f, IRInstr *i)
{
	CodeGen *c = f->cg;
	IRInstr *ret = tail_ret(i);
	if (!tail_match(f, i, ret)) return 0;

	if (c->ir->fns + i->target == f->fn) {
		cg_printf(c, "\tjmp .LS%u\n", i->target);
	} else {
		move_saved(f, f->ctx.callee_used, 1);
		if (!f->leaf) cg_printf(c, "\tleave\n");
		cg_printf(c, "\tjmp ");
		print_fn_name(c, c->ir->fns + i->target);
		cg_printf(c, "\n");
	}

	return ret == i + 1 ? 2 : 1;
}

static void make_select (X86Fn *f, IRInstr *i)
{
	test_zero(f, i->cond);
	select_move(f, i, "ne", "e");
}

static int make_instr (X86Fn *f, IRInstr *i)
{
	if (is_silent(f, i)) return 1;
	begin_instr(f, i);
	if (try_rmw_inplace(f, i)) return 1;
	int n;

	switch (i->op)
	{
	case IR_CONST:
		if (f->regs[i->dst].cstate != VR_CONST) store_imm(f, i->dst, i->imm64);
		return 1;
	case IR_PARAM: make_param(f, i); return 1;

	case IR_LD_GLOBAL:
		n = make_rmw(f, i);
		if (n) return n;
		make_ld_global(f, i);
		return 1;
	case IR_STR_GLOBAL: make_str_global(f, i); return 1;

	case IR_MOVE:
		n = make_rmw(f, i);
		if (n) return n;
		make_move(f, i);
		return 1;
	case IR_EXTEND: make_move(f, i); return 1;

	case IR_ADD: case IR_SUB: case IR_AND_A: case IR_OR_A: case IR_MUL: case IR_XOR:
		make_arith(f, i); return 1;
	case IR_DIV: case IR_MOD: make_divmod(f, i); return 1;
	case IR_RS: case IR_LS: make_shift(f, i); return 1;

	case IR_NEG: make_unary(f, i, "neg"); return 1;
	case IR_NOT_A: make_unary(f, i, "not"); return 1;
	case IR_NOT_L: make_not_l(f, i); return 1;

	case IR_SELECT: make_select(f, i); return 1;
	case IR_EQ: case IR_NE: case IR_GT: case IR_GE: case IR_LT: case IR_LE:
		return make_cmp_instr(f, i);

	case IR_LABEL: case IR_JMP: case IR_JZ: case IR_JNZ: make_jump(f, i); return 1;

	case IR_ARG: make_arg(f, i); return 1;
	case IR_CALL:
		n = make_tail_call(f, i);
		if (n) return n;
		make_call(f, i);
		return 1;
	case IR_RET: make_ret(f, i); return 1;

	default: return 1;
	}
}

static void make_body (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	IRInstr *instr, *last;
	uint32_t i = 0, n;
	int dead = 0;

	while (i < fn->count) {
		instr = ir->instrs + fn->start + i;
		if (instr->op == IR_LABEL) dead = 0;
		if (dead) {
			i++;
			continue;
		}
		n = make_instr(f, instr);
		last = instr + n - 1;
		dead = last->op == IR_RET || last->op == IR_JMP;
		i += n;
	}
}

static void make_fn (X86Fn *f)
{
	CodeGen *c = f->cg;
	IRFn *fn = f->fn;

	cg_printf(c, "\t.p2align 4\n\t.globl ");
	print_fn_name(c, fn);
	cg_printf(c, "\n\t.type ");
	print_fn_name(c, fn);
	cg_printf(c, ", @function\n");
	print_fn_name(c, fn);

	cg_printf(c, ":\n");
	if (!f->leaf) cg_printf(c, "\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (f->frame) cg_printf(c, "\tsubq $%u, %%rsp\n", f->frame);
	move_saved(f, f->ctx.callee_used, 0);
	cg_printf(c, ".LS%u:\n", (uint32_t)(fn - c->ir->fns));

	make_body(f);

	cg_printf(c, "\t.size ");
	print_fn_name(c, fn);
	cg_printf(c, ", .-");
	print_fn_name(c, fn);
	cg_printf(c, "\n");
}

static uint32_t find_main (IR *ir)
{
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (ir->fns[i].len == 4 && !strncmp(ir->fns[i].name, "main", 4)) return i;
	return NO_REG;
}

static void make_entry (CodeGen *c, int run_init)
{
	if (find_main(c->ir) == NO_REG) return;
	if (run_init) {
		cg_printf(c, "\t.globl main\nmain:\n\tsubq $8, %%rsp\n");
		cg_printf(c, "\tcall " INIT_SYM "\n\taddq $8, %%rsp\n");
		cg_printf(c, "\tjmp " SYM_PREFIX "main\n");
		return;
	}
	cg_printf(c, "\t.globl main\nmain:\n\tjmp " SYM_PREFIX "main\n");
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;

	uint32_t max_regs = 1;
	uint32_t max_instrs = 1;
	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (ir->fns[i].reg_count > max_regs)
			max_regs = ir->fns[i].reg_count;
		if (ir->fns[i].count > max_instrs)
			max_instrs = ir->fns[i].count;
	}

	X86Fn f = {0};
	f.cg = c;

	f.regs = arena_alloc(c->arena, (size_t)max_regs * sizeof(Reg));
	f.blocks = arena_alloc(c->arena, (size_t)max_instrs * sizeof(uint32_t));
	f.saves = arena_alloc(c->arena, (size_t)(max_instrs + 1) * sizeof(uint64_t));
	f.frees = arena_alloc(c->arena, (size_t)(max_instrs + 1) * sizeof(uint64_t));

	int init_empty = ir->fns[ir->init_fn].count <= 1;
	make_globals(c);
	cg_printf(c, "\t.text\n");

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (init_empty && i == ir->init_fn) continue;
		f.fn = ir->fns + i;
		layout_fn(&f);
		check_regalloc(&f);
		make_fn(&f);
	}
	make_entry(c, !init_empty);

	cg_printf(c, "\t.section .note.GNU-stack,\"\",@progbits\n");

	return 0;
}
