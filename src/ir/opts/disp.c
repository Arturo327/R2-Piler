#include "ir/opts/common.h"

static int disp_candidate (Optimizer *opt, OptFn *f, IRInstr *add, int64_t *k)
{

	if (add->op != IR_ADD && add->op != IR_SUB) return 0;
	if (add->dst == NO_REG || add->dst == add->src1) return 0;
	if (types[add->data_type].size != 8) return 0;
	if (f->defs[add->dst] != 1 || reg_addressed(f->fn, add->dst)) return 0;

	int64_t c;
	if (!reg_const(opt, f, add->src2, &c)) return 0;
	*k = add->op == IR_SUB ? -c : c;
	return 1;
}

static int disp_forward (OptFn *f, IRInstr *code, uint32_t n, IRInstr *add, int64_t k)
{
	uint32_t pending = f->uses[add->dst];
	int changed = 0;

	for (uint32_t j = 0; j < n && pending; j++) {
		IRInstr *in = code + j;
		uint32_t *src[3];
		int cnt = get_srcs(in, src);

		for (int s = 0; s < cnt; s++) {
			if (*src[s] == add->dst) pending--;
		}
		if ((in->op == IR_LD_PTR || in->op == IR_STR_PTR) && in->src1 == add->dst
				&& in->imm64 + k == (int32_t)(in->imm64 + k)) {
			in->src1 = add->src1;
			in->imm64 += k;
			changed = 1;
		}
		if (in->dst == add->src1) break;
	}
	return changed;
}

int opt_disp (Optimizer *opt, OptFn *f)
{
	int changed = 0;

	for (uint32_t b = 0; b < f->block_count; b++) {
		IRInstr *code = opt->ir->instrs + f->blocks[b].start;
		uint32_t n = f->blocks[b].count;

		f->epoch++;
		for (uint32_t i = 0; i < n; i++) {
			int64_t k;

			if (code[i].op == IR_NOP) continue;
			if (disp_candidate(opt, f, code + i, &k))
				changed |= disp_forward(f, code + i + 1, n - i - 1, code + i, k);
			track_const(f, code + i);
		}
	}
	f->epoch++;
	return changed;
}
