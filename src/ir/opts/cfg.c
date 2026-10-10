#include "ir/opts/common.h"

#define OPT_MAX_HOPS 8

int opt_unreachable (Optimizer *opt, OptFn *f)
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

int opt_jumps (Optimizer *opt, OptFn *f)
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
