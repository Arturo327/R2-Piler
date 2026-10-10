#include "ir/opt.h"

#include <string.h>

#define OPT_MAX_ROUNDS 8
#define OPT_MAX_HOPS 8

typedef int (*OptPass) (Optimizer *opt, OptFn *f);

typedef struct OptPassDesc {
	OptPass run;
	OptLevel min_level;
} OptPassDesc;

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

static void fill_blocks (IR *ir, OptFn *f)
{
	uint32_t b = 0;

	f->blocks[0].start = f->fn->start;
	f->blocks[0].count = 0;
	for (uint32_t i = 0; i < f->fn->count; i++) {
		if (i > 0 && starts_block(ir, f->fn, i)) {
			b++;
			f->blocks[b].start = f->fn->start + i;
			f->blocks[b].count = 0;
		}
		f->blocks[b].count++;
	}
	f->block_count = b + 1;
}

static void build_lmap (IR *ir, OptFn *f)
{
	for (uint32_t j = 0; j < f->block_count; j++) {
		IRInstr *in = ir->instrs + f->blocks[j].start;

		if (in->op != IR_LABEL) continue;
		f->lmap[in->target] = j;
		f->lrefs[in->target] = 0;
	}
}

static void count_regs (IR *ir, OptFn *f)
{
	IRFn *fn = f->fn;
	memset(f->defs, 0, (size_t)fn->reg_count * sizeof(uint32_t));
	memset(f->uses, 0, (size_t)fn->reg_count * sizeof(uint32_t));

	for (uint32_t j = 0; j < fn->count; j++) {
		IRInstr *in = ir->instrs + fn->start + j;

		if (in->op == IR_NOP) continue;
		if (in->dst != NO_REG) {
			f->defs[in->dst]++;
			f->def_at[in->dst] = fn->start + j;
		}
		if (in->src1 != NO_REG)
			f->uses[in->src1]++;
		if (in->src2 != NO_REG)
			f->uses[in->src2]++;
	}
}

static void count_label_refs (IR *ir, OptFn *f)
{
	for (uint32_t j = 0; j < f->fn->count; j++) {
		IRInstr *in = ir->instrs + f->fn->start + j;

		if (in->op == IR_JMP || in->op == IR_JZ || in->op == IR_JNZ)
			f->lrefs[in->target]++;
	}
}

static void analyze_fn (Optimizer *opt, OptFn *f)
{
	fill_blocks(opt->ir, f);
	build_lmap(opt->ir, f);
	count_regs(opt->ir, f);
	count_label_refs(opt->ir, f);
}

static uint32_t label_block (IR *ir, OptFn *f, uint32_t label)
{
	uint32_t b = f->lmap[label];
	IRInstr *in;

	if (b >= f->block_count) return OPT_NO_BLOCK;
	in = ir->instrs + f->blocks[b].start;
	return in->op == IR_LABEL && in->target == label ? b : OPT_NO_BLOCK;
}

static void build_cfg (IR *ir, OptFn *f)
{
	for (uint32_t b = 0; b < f->block_count; b++) {
		OptBlock *blk = f->blocks + b;
		IRInstr *last = ir->instrs + blk->start + blk->count - 1;
		int jumps = last->op == IR_JMP || last->op == IR_JZ || last->op == IR_JNZ;
		int falls = last->op != IR_JMP && last->op != IR_RET && b + 1 < f->block_count;

		blk->succ_count = 0;
		blk->reachable = 0;
		if (falls) blk->succ[blk->succ_count++] = b + 1;
		if (!jumps) continue;

		uint32_t t = label_block(ir, f, last->target);
		if (t != OPT_NO_BLOCK) blk->succ[blk->succ_count++] = t;
	}
}

static void mark_reachable (OptFn *f)
{
	uint32_t sp = 0;

	f->work[sp++] = 0;
	f->blocks[0].reachable = 1;
	while (sp) {
		OptBlock *b = f->blocks + f->work[--sp];

		for (uint32_t k = 0; k < b->succ_count; k++) {
			if (f->blocks[b->succ[k]].reachable) continue;
			f->blocks[b->succ[k]].reachable = 1;
			f->work[sp++] = b->succ[k];
		}
	}
}

static void kill_instr (IRInstr *in)
{
	in->op = IR_NOP;
	in->dst = NO_REG;
	in->src1 = NO_REG;
	in->src2 = NO_REG;
	in->argc = 0;
}

static IRInstr *next_real (IRInstr *code, uint32_t from, uint32_t count)
{
	while (from < count && code[from].op == IR_NOP) from++;
	return from < count ? code + from : NULL;
}

static int same_rep (IRFn *fn, uint32_t a, uint32_t b)
{
	const Type *ta = types + fn->reg_types[a];
	const Type *tb = types + fn->reg_types[b];

	return ta->size == tb->size && (ta->size == 8 || ta->sign == tb->sign);
}

static int64_t norm_val (uint64_t v, uint8_t type)
{
	const Type *t = types + type;
	unsigned sh = 64u - t->size * 8u;

	v <<= sh;
	return t->sign ? (int64_t)v >> sh : (int64_t)(v >> sh);
}

static int forward_copy (IRInstr *code, uint32_t n, IRInstr *mv, uint32_t pending)
{
	int changed = 0;

	for (uint32_t k = 0; k < n && pending; k++) {
		IRInstr *in = code + k;

		if (in->op == IR_NOP) continue;
		if (in->src1 == mv->dst) {
			in->src1 = mv->src1;
			changed = 1;
			pending--;
		}
		if (in->src2 == mv->dst) {
			in->src2 = mv->src1;
			changed = 1;
			pending--;
		}
		if (in->dst == mv->src1) break;
	}
	return changed;
}

static int opt_propagate (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++) {
		IRInstr *code = opt->ir->instrs + f->blocks[b].start;
		uint32_t n = f->blocks[b].count;

		for (uint32_t i = 0; i < n; i++) {
			IRInstr *mv = code + i;

			if (mv->op != IR_MOVE || f->defs[mv->dst] != 1) continue;
			if (mv->dst == mv->src1 || !same_rep(f->fn, mv->dst, mv->src1)) continue;
			changed |= forward_copy(code + i + 1, n - i - 1, mv, f->uses[mv->dst]);
		}
	}
	return changed;
}

static int reg_const (Optimizer *opt, OptFn *f, uint32_t reg, int64_t *val)
{
	IRInstr *def;

	if (f->kstamp[reg] == f->epoch) {
		*val = f->kval[reg];
		return 1;
	}
	if (f->defs[reg] != 1) return 0;
	def = opt->ir->instrs + f->def_at[reg];
	if (def->op != IR_CONST) return 0;

	*val = norm_val((uint64_t)def->imm64, f->fn->reg_types[reg]);
	return 1;
}

static int eval_divmod (uint8_t op, int sign, int64_t a, int64_t b, int64_t *r)
{
	if (b == 0 || (sign && b == -1)) return 0;
	if (sign) {
		*r = op == IR_DIV ? a / b : a % b;
		return 1;
	}
	if (op == IR_DIV) *r = (int64_t)((uint64_t)a / (uint64_t)b);
	else *r = (int64_t)((uint64_t)a % (uint64_t)b);
	return 1;
}

static int eval_compare (uint8_t op, int sign, int64_t a, int64_t b, int64_t *r)
{
	uint64_t ua = (uint64_t)a;
	uint64_t ub = (uint64_t)b;

	switch (op)
	{
	case IR_EQ: *r = a == b; return 1;
	case IR_NE: *r = a != b; return 1;
	case IR_GT: *r = sign ? a > b : ua > ub; return 1;
	case IR_GE: *r = sign ? a >= b : ua >= ub; return 1;
	case IR_LT: *r = sign ? a < b : ua < ub; return 1;
	case IR_LE: *r = sign ? a <= b : ua <= ub; return 1;
	default: return 0;
	}
}

static int eval_binary (uint8_t op, uint8_t type, int64_t a, int64_t b, int64_t *r)
{
	int sign = types[type].sign;
	uint64_t ua = (uint64_t)a;
	uint64_t ub = (uint64_t)b;

	switch (op)
	{
	case IR_ADD: *r = (int64_t)(ua + ub); return 1;
	case IR_SUB: *r = (int64_t)(ua - ub); return 1;
	case IR_MUL: *r = (int64_t)(ua * ub); return 1;
	case IR_AND_A: *r = (int64_t)(ua & ub); return 1;
	case IR_OR_A: *r = (int64_t)(ua | ub); return 1;
	case IR_XOR: *r = (int64_t)(ua ^ ub); return 1;
	case IR_LS:
		if (ub >= 64) return 0;
		*r = (int64_t)(ua << ub);
		return 1;
	case IR_RS:
		if (ub >= 64) return 0;
		*r = sign ? a >> ub : (int64_t)(ua >> ub);
		return 1;
	case IR_DIV: case IR_MOD: return eval_divmod(op, sign, a, b, r);
	default: return eval_compare(op, sign, a, b, r);
	}
}

static int eval_unary (uint8_t op, int64_t a, int64_t *r)
{
	switch (op)
	{
	case IR_MOVE: case IR_EXTEND: *r = a; return 1;
	case IR_NEG: *r = (int64_t)(0 - (uint64_t)a); return 1;
	case IR_NOT_A: *r = ~a; return 1;
	case IR_NOT_L: *r = a == 0; return 1;
	default: return 0;
	}
}

static void to_const (OptFn *f, IRInstr *in, int64_t v)
{
	in->data_type = f->fn->reg_types[in->dst];
	in->imm64 = norm_val((uint64_t)v, in->data_type);
	in->op = IR_CONST;
	in->src1 = NO_REG;
	in->src2 = NO_REG;
}

static int try_move (OptFn *f, IRInstr *in, uint32_t src)
{
	if (!same_rep(f->fn, in->dst, src)) return 0;
	if (in->dst == src) {
		kill_instr(in);
		return 1;
	}
	in->op = IR_MOVE;
	in->src1 = src;
	in->src2 = NO_REG;
	return 1;
}

static int fold_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	int64_t a, b, r = 0;
	int ok = 0;

	if (in->dst == NO_REG || in->src1 == NO_REG) return 0;
	if (!reg_const(opt, f, in->src1, &a)) return 0;

	if (in->src2 == NO_REG) ok = eval_unary(in->op, a, &r);
	else if (reg_const(opt, f, in->src2, &b))
		ok = eval_binary(in->op, in->data_type, a, b, &r);
	if (!ok) return 0;

	to_const(f, in, r);
	return 1;
}

static int simplify_same (OptFn *f, IRInstr *in)
{
	switch (in->op)
	{
	case IR_SUB: case IR_XOR: case IR_NE: case IR_LT: case IR_GT:
		to_const(f, in, 0);
		return 1;
	case IR_EQ: case IR_LE: case IR_GE:
		to_const(f, in, 1);
		return 1;
	case IR_AND_A: case IR_OR_A:
		return try_move(f, in, in->src1);
	default:
		return 0;
	}
}

static int simplify_rhs (OptFn *f, IRInstr *in, int64_t b)
{
	switch (in->op)
	{
	case IR_ADD: case IR_SUB: case IR_OR_A: case IR_XOR: case IR_LS: case IR_RS:
		return b == 0 && try_move(f, in, in->src1);
	case IR_DIV:
		return b == 1 && try_move(f, in, in->src1);
	case IR_MOD:
		if (b != 1) return 0;
		to_const(f, in, 0);
		return 1;
	case IR_MUL:
		if (b != 0) return b == 1 && try_move(f, in, in->src1);
		to_const(f, in, 0);
		return 1;
	case IR_AND_A:
		if (b != 0) return b == norm_val(~(uint64_t)0, in->data_type)
				&& try_move(f, in, in->src1);
		to_const(f, in, 0);
		return 1;
	default:
		return 0;
	}
}

static int simplify_lhs (OptFn *f, IRInstr *in, int64_t a)
{
	switch (in->op)
	{
	case IR_ADD: case IR_OR_A: case IR_XOR:
		return a == 0 && try_move(f, in, in->src2);
	case IR_MUL:
		if (a != 0) return a == 1 && try_move(f, in, in->src2);
		to_const(f, in, 0);
		return 1;
	case IR_AND_A:
		if (a != 0) return a == norm_val(~(uint64_t)0, in->data_type)
				&& try_move(f, in, in->src2);
		to_const(f, in, 0);
		return 1;
	case IR_LS: case IR_RS:
		if (a != 0) return 0;
		to_const(f, in, 0);
		return 1;
	default:
		return 0;
	}
}

static int simplify_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	if (in->dst == NO_REG || in->src1 == NO_REG || in->src2 == NO_REG) return 0;
	if (in->src1 == in->src2) return simplify_same(f, in);

	int64_t a = 0;
	int64_t b = 0;

	int ca = reg_const(opt, f, in->src1, &a);
	int cb = reg_const(opt, f, in->src2, &b);

	if (ca && cb) return 0;
	if (cb) return simplify_rhs(f, in, b);
	if (ca) return simplify_lhs(f, in, a);
	return 0;
}

static int fold_jump (Optimizer *opt, OptFn *f, IRInstr *in)
{
	int64_t v;

	if (!reg_const(opt, f, in->src1, &v)) return 0;
	if ((v != 0) == (in->op == IR_JNZ)) {
		in->op = IR_JMP;
		in->src1 = NO_REG;
	} else kill_instr(in);
	return 1;
}

static void track_const (OptFn *f, IRInstr *in)
{
	if (in->dst == NO_REG) return;
	if (in->op != IR_CONST) {
		f->kstamp[in->dst] = 0;
		return;
	}
	f->kstamp[in->dst] = f->epoch;
	f->kval[in->dst] = norm_val((uint64_t)in->imm64, f->fn->reg_types[in->dst]);
}

static int fold_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->epoch++;
	for (uint32_t i = 0; i < blk->count; i++) {
		IRInstr *in = code + i;

		if (in->op == IR_JZ || in->op == IR_JNZ)
			changed |= fold_jump(opt, f, in);
		else changed |= fold_instr(opt, f, in) || simplify_instr(opt, f, in);
		track_const(f, in);
	}
	return changed;
}

static int opt_fold (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= fold_block(opt, f, f->blocks + b);
	f->epoch++;
	return changed;
}

static int opt_coalesce (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = 0; i < f->fn->count; i++) {
		IRInstr *a = code + i;
		IRInstr *m;
		int can = (is_deletable[a->op] && a->op != IR_MOVE)
				|| a->op == IR_CALL || a->op == IR_DIV || a->op == IR_MOD;

		if (!can || a->dst == NO_REG) continue;
		if (f->defs[a->dst] != 1 || f->uses[a->dst] != 1) continue;

		m = next_real(code, i + 1, f->fn->count);
		if (!m || m->op != IR_MOVE || m->src1 != a->dst || m->dst == a->dst) continue;
		if (!same_rep(f->fn, a->dst, m->dst)) continue;

		a->dst = m->dst;
		kill_instr(m);
		changed = 1;
	}
	return changed;
}

static int opt_dce (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = f->fn->count; i-- > 0;) {
		IRInstr *in = code + i;

		if (in->dst == NO_REG || f->uses[in->dst] || !is_deletable[in->op]) continue;
		if (in->src1 != NO_REG) f->uses[in->src1]--;
		if (in->src2 != NO_REG) f->uses[in->src2]--;
		kill_instr(in);
		changed = 1;
	}
	return changed;
}

static int opt_unreachable (Optimizer *opt, OptFn *f)
{
	IRInstr *last = opt->ir->instrs + f->fn->start + f->fn->count - 1;
	int changed = 0;

	build_cfg(opt->ir, f);
	mark_reachable(f);
	for (uint32_t b = 1; b < f->block_count; b++) {
		IRInstr *code = opt->ir->instrs + f->blocks[b].start;

		if (f->blocks[b].reachable) continue;
		for (uint32_t i = 0; i < f->blocks[b].count; i++) {
			if (code[i].op == IR_NOP || code + i == last) continue;
			kill_instr(code + i);
			changed = 1;
		}
	}
	return changed;
}

static int flip_branch (OptFn *f, IRInstr *code, uint32_t i)
{
	IRInstr *br = code + i;
	IRInstr *jmp = next_real(code, i + 1, f->fn->count);
	IRInstr *lab;

	if (!jmp || jmp->op != IR_JMP) return 0;
	lab = next_real(code, (uint32_t)(jmp - code) + 1, f->fn->count);
	if (!lab || lab->op != IR_LABEL || lab->target != br->target) return 0;

	f->lrefs[br->target]--;
	br->op = br->op == IR_JZ ? IR_JNZ : IR_JZ;
	br->target = jmp->target;
	kill_instr(jmp);
	return 1;
}

static IRInstr *skip_labels (IRInstr *code, uint32_t from, uint32_t count)
{
	IRInstr *in = next_real(code, from, count);

	while (in && in->op == IR_LABEL)
		in = next_real(code, (uint32_t)(in - code) + 1, count);
	return in;
}

static int thread_jump (Optimizer *opt, OptFn *f, IRInstr *br)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	uint32_t label = br->target;

	for (int hops = 0; hops < OPT_MAX_HOPS; hops++) {
		uint32_t b = label_block(opt->ir, f, label);
		IRInstr *nx;

		if (b == OPT_NO_BLOCK) break;
		nx = skip_labels(code, f->blocks[b].start - f->fn->start + 1,
				f->fn->count);
		if (!nx || nx->op != IR_JMP || nx->target == label) break;
		label = nx->target;
	}
	if (label == br->target) return 0;
	f->lrefs[br->target]--;
	f->lrefs[label]++;
	br->target = label;
	return 1;
}

static int jump_to_next (OptFn *f, IRInstr *code, uint32_t i)
{
	IRInstr *in = code + i;
	IRInstr *nx = next_real(code, i + 1, f->fn->count);

	while (nx && nx->op == IR_LABEL) {
		if (nx->target == in->target) {
			f->lrefs[in->target]--;
			kill_instr(in);
			return 1;
		}
		nx = next_real(code, (uint32_t)(nx - code) + 1, f->fn->count);
	}
	return 0;
}

static int opt_jumps (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = 0; i + 1 < f->fn->count; i++) {
		IRInstr *in = code + i;

		if (in->op == IR_LABEL && !f->lrefs[in->target]) {
			kill_instr(in);
			changed = 1;
		} else if (in->op == IR_JMP) {
			changed |= thread_jump(opt, f, in);
			changed |= jump_to_next(f, code, i);
		} else if (in->op == IR_JZ || in->op == IR_JNZ) {
			changed |= thread_jump(opt, f, in);
			changed |= flip_branch(f, code, i);
		}
	}
	return changed;
}

static int strength_instr (Optimizer *opt, OptFn *f, IRInstr *in)
{
	IRInstr *k;
	uint64_t v;

	if (in->dst == NO_REG || in->src2 == NO_REG) return 0;
	if (in->op != IR_MUL && in->op != IR_DIV && in->op != IR_MOD) return 0;
	if (f->defs[in->src2] != 1 || f->uses[in->src2] != 1) return 0;

	k = opt->ir->instrs + f->def_at[in->src2];
	if (k->op != IR_CONST) return 0;
	v = (uint64_t)norm_val((uint64_t)k->imm64, f->fn->reg_types[in->src2]);
	if (v < 2 || (v & (v - 1)) != 0) return 0;
	if (in->op != IR_MUL && types[in->data_type].sign) return 0;

	if (in->op == IR_MUL) {
		in->op = IR_LS;
		k->imm64 = __builtin_ctzll(v);
	} else if (in->op == IR_DIV) {
		in->op = IR_RS;
		k->imm64 = __builtin_ctzll(v);
	} else {
		in->op = IR_AND_A;
		k->imm64 = (int64_t)(v - 1);
	}
	return 1;
}

static int opt_strength (Optimizer *opt, OptFn *f)
{
	IRInstr *code = opt->ir->instrs + f->fn->start;
	int changed = 0;

	for (uint32_t i = 0; i < f->fn->count; i++)
		changed |= strength_instr(opt, f, code + i);

	return changed;
}

static int fwd_global (OptFn *f, IRInstr *in)
{
	if (f->gstamp[in->target] != f->gepoch) return 0;

	uint32_t r = f->gval[in->target];
	if (f->fn->reg_types[in->dst] != f->fn->reg_types[r]) return 0;

	in->op = IR_MOVE;
	in->src1 = r;
	in->src2 = NO_REG;
	in->data_type = f->fn->reg_types[r];
	return 1;
}

static void note_global (OptFn *f, IRInstr *in, uint32_t reg)
{
	uint32_t g = in->target;

	f->gstamp[g] = 0;
	if (f->defs[reg] != 1) return;
	if (f->fn->reg_types[reg] != in->data_type) return;
	f->gval[g] = reg;
	f->gstamp[g] = f->gepoch;
}

static int globals_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->gepoch++;
	for (uint32_t i = 0; i < blk->count; i++) {
		IRInstr *in = code + i;

		if (in->op == IR_CALL) f->gepoch++;
		else if (in->op == IR_STR_GLOBAL) note_global(f, in, in->src1);
		else if (in->op == IR_LD_GLOBAL && fwd_global(f, in)) changed = 1;
		else if (in->op == IR_LD_GLOBAL) note_global(f, in, in->dst);
	}
	return changed;
}

static int opt_globals (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= globals_block(opt, f, f->blocks + b);
	return changed;
}

static int dse_block (Optimizer *opt, OptFn *f, OptBlock *blk)
{
	IRInstr *code = opt->ir->instrs + blk->start;
	int changed = 0;

	f->depoch++;
	uint32_t i = blk->count;
	while (i-- > 0) {
		IRInstr *in = code + i;

		if (in->op == IR_NOP) continue;
		if (in->dst != NO_REG && f->dstamp[in->dst] == f->depoch
				&& is_deletable[in->op]) {
			kill_instr(in);
			changed = 1;
			continue;
		}
		if (in->dst != NO_REG) f->dstamp[in->dst] = f->depoch;
		if (in->src1 != NO_REG) f->dstamp[in->src1] = 0;
		if (in->src2 != NO_REG) f->dstamp[in->src2] = 0;
	}
	return changed;
}

static int opt_dse (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++)
		changed |= dse_block(opt, f, f->blocks + b);
	return changed;
}

static const OptPassDesc passes[] = {
	{ opt_propagate, OPT_BASIC },
	{ opt_fold, OPT_BASIC },
	{ opt_coalesce, OPT_BASIC },
	{ opt_dce, OPT_BASIC },
	{ opt_unreachable, OPT_BASIC },
	{ opt_jumps, OPT_BASIC },
	{ opt_strength, OPT_BASIC },
	{ opt_globals, OPT_BASIC },
	{ opt_dse, OPT_BASIC },
};

static void optimize_fn (Optimizer *opt, OptFn *f)
{
	size_t n = sizeof(passes) / sizeof(passes[0]);

	for (int round = 0; round < OPT_MAX_ROUNDS; round++) {
		int changed = 0;

		for (size_t p = 0; p < n; p++) {
			if (passes[p].min_level > opt->level) continue;
			analyze_fn(opt, f);
			changed |= passes[p].run(opt, f);
		}
		if (!changed) break;
	}
}

static void compact_ir (IR *ir)
{
	uint32_t out = 0;

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		IRFn *fn = ir->fns + i;
		uint32_t begin = fn->start;
		uint32_t end = begin + fn->count;

		fn->start = out;
		for (uint32_t j = begin; j < end; j++)
			if (ir->instrs[j].op != IR_NOP) ir->instrs[out++] = ir->instrs[j];
		fn->count = out - fn->start;
	}
	ir->instr_count = out;
}

static void init_caches (OptFn *f, IR *ir, Arena *a, size_t max_regs)
{
	size_t globals = (size_t)ir->global_count + 1;

	f->gval = arena_alloc(a, globals * sizeof(uint32_t));
	f->gstamp = arena_alloc(a, globals * sizeof(uint32_t));
	f->dstamp = arena_alloc(a, max_regs * sizeof(uint32_t));
	memset(f->gstamp, 0, globals * sizeof(uint32_t));
	memset(f->dstamp, 0, max_regs * sizeof(uint32_t));
	f->gepoch = 1;
	f->depoch = 1;
}

static void init_opt (Optimizer *opt, IR *ir, Arena *a, OptLevel level)
{
	size_t max_regs = 1;
	size_t max_instrs = 1;
	size_t labels = (size_t)ir->label_count + 1;

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (ir->fns[i].reg_count > max_regs) max_regs = ir->fns[i].reg_count;
		if (ir->fns[i].count > max_instrs) max_instrs = ir->fns[i].count;
	}
	opt->arena = a;
	opt->ir = ir;
	opt->level = level;
	opt->lmap = arena_alloc(a, labels * sizeof(uint32_t));
	opt->lrefs = arena_alloc(a, labels * sizeof(uint32_t));
	memset(opt->lmap, 0xFF, labels * sizeof(uint32_t));

	opt->cur.lmap = opt->lmap;
	opt->cur.lrefs = opt->lrefs;
	opt->cur.defs = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.uses = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.def_at = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.blocks = arena_alloc(a, max_instrs * sizeof(OptBlock));
	opt->cur.work = arena_alloc(a, max_instrs * sizeof(uint32_t));

	opt->cur.kstamp = arena_alloc(a, max_regs * sizeof(uint32_t));
	opt->cur.kval = arena_alloc(a, max_regs * sizeof(int64_t));
	memset(opt->cur.kstamp, 0, max_regs * sizeof(uint32_t));
	opt->cur.epoch = 1;

	init_caches(&opt->cur, ir, a, max_regs);
}

static const uint8_t stops_static[IR_COUNT] = {
	[IR_CALL] = 1, [IR_JMP] = 1, [IR_JZ] = 1, [IR_JNZ] = 1, [IR_LABEL] = 1
};

static int bind_static (Optimizer *opt, OptFn *f, IRInstr *st)
{
	IRInstr *first = opt->ir->instrs + f->fn->start;
	IRGlobal *g = opt->ir->globals + st->target;
	IRInstr *cst;

	if (st == first) return 0;
	cst = st - 1;
	while (cst > first && cst->op == IR_NOP) cst--;
	if (cst->op != IR_CONST || cst->dst != st->src1) return 0;

	if (types[cst->data_type].size != types[g->type].size) return 0;
	if (types[g->type].size < 8 && cst->data_type != g->type) return 0;

	g->has_init = 1;
	g->init = cst->imm64;
	if (f->uses[cst->dst] == 1) kill_instr(cst);
	kill_instr(st);
	return 1;
}

static int scan_static_init (Optimizer *opt, OptFn *f)
{
	IR *ir = opt->ir;
	uint8_t *seen = arena_alloc(opt->arena, ir->global_count + 1);
	int changed = 0;

	memset(seen, 0, ir->global_count + 1);
	for (uint32_t i = 0; i < f->fn->count; i++) {
		IRInstr *in = ir->instrs + f->fn->start + i;

		if (stops_static[in->op]) break;
		if (in->op != IR_STR_GLOBAL && in->op != IR_LD_GLOBAL) continue;
		if (seen[in->target]) continue;
		seen[in->target] = 1;
		if (in->op == IR_STR_GLOBAL) changed |= bind_static(opt, f, in);
	}
	return changed;
}

static void run_static_init (Optimizer *opt)
{
	OptFn *f = &opt->cur;

	f->fn = opt->ir->fns + opt->ir->init_fn;
	analyze_fn(opt, f);
	if (scan_static_init(opt, f)) optimize_fn(opt, f);
}

void optimize_ir (IR *ir, Arena *a, OptLevel level)
{
	Optimizer opt;

	if (level == NO_OPT) return;
	init_opt(&opt, ir, a, level);
	for (uint32_t i = 0; i < ir->fn_count; i++) {
		opt.cur.fn = ir->fns + i;
		optimize_fn(&opt, &opt.cur);
	}
	run_static_init(&opt);
	compact_ir(ir);
}
