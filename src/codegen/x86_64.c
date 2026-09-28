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

static uint32_t lay_size (X86Fn *f, uint32_t c, uint8_t s)
{
	IRFn *fn = f->fn;
	for (uint32_t r = 0; r < fn->reg_count; r++) {
		if (types[fn->reg_types[r]].size != s) continue;
		c += s;
		f->slots[r] = c;
	}
	return c;
}

static uint32_t scan_outg (X86Fn *f)
{
	IR *ir = f->cg->ir;
	IRFn *fn = f->fn;
	uint32_t out = 0;

	for (uint32_t i = 0; i < fn->count; i++) {
		IRInstr *instr = ir->instrs + fn->start + i;
		uint32_t n;
		if (instr->op != IR_CALL || instr->argc <= 6) continue;
		n = (instr->argc - 6) << 3;
		if (n > out) out = n;
	}
	return out;
}

static void layout_fn (X86Fn *f)
{
	uint32_t c = lay_size(f, 0, 8);
	c = lay_size(f, c, 4);
	c = lay_size(f, c, 2);
	c = lay_size(f, c, 1);
	f->outgoing = scan_outg(f);
	c += f->outgoing;
	f->frame = (c + 15) & ~15u;
}

static void print_fn_name (CodeGen *c, IRFn *fn)
{
	if (strncmp(fn->name, "main", fn->len)) cg_printf(c, "%.*s", (int)fn->len, fn->name);
	else cg_printf(c, "__r2_main");
}

static void make_fn (X86Fn *f)
{
	CodeGen *c = f->cg;
//	IR *ir = c->ir;
	IRFn *fn = f->fn;

	cg_printf(c, "\t.globl ");
	print_fn_name(c, fn);
	cg_printf(c, "\n");
	print_fn_name(c, fn);

	cg_printf(c, ":\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (f->frame) cg_printf(c, "\tsubq $%u, %%rsp\n", f->frame);

//	for (uint32_t i = 0; i < fn->count; i++)
//		make_fn_instr(f, ir->instrs + fn->start + i);
}

static uint32_t find_main (IR *ir)
{
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (!strncmp(ir->fns[i].name, "main", ir->fns[i].len)) return i;
	return NO_REG;
}

static void make_entry (CodeGen *c)
{
	IR *ir = c->ir;
	if (find_main(ir) == NO_REG) return;
	cg_printf(c, "\t.globl main\nmain:\n\tpushq %%rbp\n\tmovq %%rsp, %%rbp\n");
	if (ir->global_count) cg_printf(c, "\tcall __r2_init\n");
	cg_printf(c, "\tcall __r2_main\n\tpopq %%rbp\n\tret\n");
}

int gen_x86_64 (CodeGen *c)
{
	IR *ir = c->ir;
	uint8_t *kind;
	int64_t *vals;

	uint32_t max_regs = 1;
	for (uint32_t i = 0; i < ir->fn_count; i++)
		if (ir->fns[i].reg_count > max_regs)
			max_regs = ir->fns[i].reg_count;

	X86Fn f = {0};
	f.slots = arena_alloc(c->arena, (size_t)max_regs * sizeof(uint32_t));
	f.cg = c;

	kind = arena_alloc(c->arena, ir->global_count ? ir->global_count : 1);
	vals = arena_alloc(c->arena, (ir->global_count ? ir->global_count : 1)
			* sizeof(int64_t));

	clasf_globals(c, kind, vals);
	make_globals(c, kind, vals);
	cg_printf(c, "\t.text\n");

	for (uint32_t i = 0; i < ir->fn_count; i++) {
		f.fn = ir->fns + i;
		layout_fn(&f);
		make_fn(&f);
	}
	make_entry(c);
	cg_printf(c, "\t.section .note.GNU-stack,\"\",@progbits\n");

	return 0;
}
