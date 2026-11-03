#include "codegen/x86-64/common.h"
#include <string.h>

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

static void gen_fn (X86Fn *f, uint32_t i, uint8_t *state, int init_empty)
{
	IR *ir = f->cg->ir;
	IRFn *fn = ir->fns + i;

	if (state[i]) return;
	if (fn->is_extern || (init_empty && i == ir->init_fn)) {
		state[i] = 2;
		return;
	}
	state[i] = 1;
	for (uint32_t k = 0; k < fn->count; k++) {
		IRInstr *in = ir->instrs + fn->start + k;
		if (in->op == IR_CALL) gen_fn(f, in->target, state, init_empty);
	}

	ArenaMark mark = arena_mark(f->cg->arena);
	f->fn = fn;
	f->clobbers = 0;
	x86_layout_fn(f);
	x86_make_fn(f);
	f->fn_clobber[i] = f->clobbers & X86_CALLER_MASK;
	state[i] = 2;
	arena_release(f->cg->arena, mark);
}

static void init_x86_fn (X86Fn *f, IR *ir)
{
	uint32_t max_regs = 1;
	uint32_t max_instrs = 1;
	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (ir->fns[i].reg_count > max_regs)
			max_regs = ir->fns[i].reg_count;
		if (ir->fns[i].count > max_instrs)
			max_instrs = ir->fns[i].count;
	}

	uint32_t max_moves = max_regs > 6 ? max_regs : 6;
	f->fn_clobber = arena_alloc(f->cg->arena,
			(size_t)(ir->fn_count + 1) * sizeof(uint64_t));

	for (uint32_t i = 0; i <= ir->fn_count; i++) f->fn_clobber[i] = X86_CALLER_MASK;

	f->regs = arena_alloc(f->cg->arena, (size_t)max_regs * sizeof(Reg));
	f->blocks = arena_alloc(f->cg->arena, (size_t)max_instrs * sizeof(uint32_t));
	f->edges = arena_alloc(f->cg->arena, (size_t)(max_instrs + 1) * sizeof(EdgeMoves));
	f->saves = arena_alloc(f->cg->arena, (size_t)(max_instrs + 1) * sizeof(uint64_t));
	f->frees = arena_alloc(f->cg->arena, (size_t)(max_instrs + 1) * sizeof(uint64_t));
	f->moves = arena_alloc(f->cg->arena, (size_t)max_moves * sizeof(PMove));
	f->acts = arena_alloc(f->cg->arena, (size_t)max_moves * sizeof(PMAction));
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;
	X86Fn f = {0};
	f.cg = c;
	init_x86_fn(&f, ir);

	uint8_t *state = arena_alloc(c->arena, ir->fn_count + 1);
	memset(state, 0, ir->fn_count + 1);

	int init_empty = ir->fns[ir->init_fn].count <= 1;
	x86_make_globals(c);
	x86_make_strings(c);
	cg_printf(c, "\t.text\n");

	for (uint32_t i = 0; i < ir->fn_count; i++)
		gen_fn(&f, i, state, init_empty);

	make_entry(c, !init_empty);
	cg_printf(c, "\t.section .note.GNU-stack,\"\",@progbits\n");

	return 0;
}
