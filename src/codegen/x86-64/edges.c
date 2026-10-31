#include "codegen/x86-64/common.h"

static uint32_t edge_move (X86Fn *f, const EdgeMove *m, PMove *out)
{
	if (m->from == m->to) return 0;
	TypeId type = f->fn->reg_types[m->vreg];

	out->from = m->from >= 0 ? m->from : PM_SLOT;
	out->to = m->to >= 0 ? m->to : PM_SLOT;
	out->slot = f->regs[m->vreg].slot;
	out->size = types[type].size;
	out->sign = types[type].sign;

	return 1;
}

void x86_make_edge (X86Fn *f, uint32_t at, int taken)
{
	EdgeMoves *e = f->ctx.edges + at;
	uint32_t n = taken ? e->taken : e->fall;
	uint32_t first = e->first + (taken ? e->fall : 0);
	uint32_t count = 0;

	for (uint32_t i = 0; i < n; i++)
		count += edge_move(f, f->ctx.moves + first + i, f->moves + count);

	x86_parallel_move(f, f->moves, count);
}

void x86_make_stubs (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *in = ir->instrs + fn->start + i;

		if (in->op != IR_JZ && in->op != IR_JNZ) continue;
		if (f->regs[in->src1].cstate == VR_CONST) continue;
		if (!f->ctx.edges[i].taken) continue;

		cg_printf(f->cg, ".LE%u:\n", f->fn->start + i);
		x86_make_edge(f, i, 1);
		cg_printf(f->cg, "\tjmp .L%u\n", in->target);
	}
}
