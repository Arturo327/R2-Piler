#include "codegen/x86_64.h"

#include <string.h>

static void clasf_globals (CodeGen *c, uint8_t *kind, int64_t *vals)
{
	IR *ir = c->ir;
	IRFn *init = ir->fns + ir->init_fn;
	memset(kind, 0, ir->global_count);

	for (uint32_t i = 0; i < init->count; i++) {
		IRInstr *instr = ir->instrs + init->start + i;
		if (instr->op != IR_STR_GLOBAL || i == 0) continue;

		IRInstr *prev = instr - 1;
		if (prev->op != IR_CONST || prev->dst != instr->src1) continue;
		if (prev->data_type != ir->globals[instr->target].type) continue;

		kind[instr->target] = 1;
		vals[instr->target] = prev->imm64;
	}
}

static const char *asm_size_name (uint8_t size)
{
	switch (size)
	{
	case 1: return ".byte";
	case 2: return ".word";
	case 4: return ".long";
	default: return ".quad";
	}
}

static void make_global_data (CodeGen *c, uint32_t i, int64_t val)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;

	cg_printf(c, "\t.align %u\n%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, "%s ", asm_size_name(size));

	if (types[g->type].sign) cg_printf(c, "%lld\n", (long long)val);
	else cg_printf(c, "%llu\n", (unsigned long long)val);
}

static void make_global_bss (CodeGen *c, uint32_t i)
{
	IRGlobal *g = c->ir->globals + i;
	uint8_t size = types[g->type].size;
	cg_printf(c, "\t.align %u\n%.*s:\t", size, (int)g->len, g->name);
	cg_printf(c, ".zero %u\n", size);
}

static void make_globals (CodeGen *c, uint8_t *kind, int64_t *vals)
{
	IR *ir = c->ir;
	int printed = 0;
	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (!kind[i]) continue;
		if (!printed) cg_printf(c, "\t.section .data\n");
		make_global_data(c, i, vals[i]);
	}
	printed = 0;
	for (uint32_t i = 0; i < ir->global_count; i++) {
		if (kind[i]) continue;
		if (!printed) cg_printf(c, "\t.section .bss\n");
		make_global_bss(c, i);
	}
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;
	uint8_t *kind;
	int64_t *vals;

//	uint32_t max_regs = 1;
//	for (uint32_t i = 0; i < ir->fn_count; i++)
//		if (ir->fns[i].reg_count > max_regs)
//			max_regs = ir->fns[i].reg_count;
//
//	X86Fn f = {0};
//	f.slots = arena_alloc(c->arena, (size_t)max_regs * sizeof(uint32_t));
//	f.cg = c;
//	This is for when the real code is compiled

	kind = arena_alloc(c->arena, ir->global_count ? ir->global_count : 1);
	vals = arena_alloc(c->arena, (ir->global_count ? ir->global_count : 1)
			* sizeof(int64_t));

	clasf_globals(c, kind, vals);
	make_globals(c, kind, vals);
	cg_printf(c, "\t.text\n");

	// TODO: compile real code (functions)

	return 0;
}
