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
	f.edges = arena_alloc(c->arena, (size_t)(max_instrs + 1) * sizeof(EdgeMoves));
	f.saves = arena_alloc(c->arena, (size_t)(max_instrs + 1) * sizeof(uint64_t));
	f.frees = arena_alloc(c->arena, (size_t)(max_instrs + 1) * sizeof(uint64_t));

	int init_empty = ir->fns[ir->init_fn].count <= 1;
	x86_make_globals(c);
	cg_printf(c, "\t.text\n");

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		if (init_empty && i == ir->init_fn) continue;
		f.fn = ir->fns + i;
		x86_layout_fn(&f);
		x86_check_regalloc(&f);
		x86_make_fn(&f);
	}
	make_entry(c, !init_empty);

	cg_printf(c, "\t.section .note.GNU-stack,\"\",@progbits\n");

	return 0;
}
