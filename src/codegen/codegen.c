#include "codegen/codegen.h"
#include "codegen/x86-64/x86_64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define CG_LINE_MAX (2 * UINT16_MAX + 256)
#define CG_BUF_INIT (2 * CG_LINE_MAX)

typedef struct Backend {
	const char *name;
	int (*generate) (CodeGen *c);
} Backend;

static const Backend backends[ARCH_COUNT] = {
	[ARCH_X86_64] = { "x86-64", gen_x86_64 },
	[ARCH_ARM] = { "ARM", NULL },
	[ARCH_RISCV] = { "RISC-V", NULL },
	[ARCH_INTERP] = { "interpreter", NULL }
};

int init_codegen (CodeGen *codegen, Arch arch, Arena *arena, IR *ir, const char *out, OptLevel level)
{
	const Backend *b = backends + arch;

	if (!b->generate) {
		fprintf(stderr, "Error: %s backend is not implemented\n", b->name);
		return 1;
	}
	codegen->arena = arena;
	codegen->ir = ir;
	codegen->out = out;
	codegen->opt_level = level;
	codegen->generate = b->generate;
	codegen->len = 0;
	codegen->cap = CG_BUF_INIT;
	codegen->code = arena_alloc(arena, codegen->cap);
	return 0;
}

static int write_output (CodeGen *c)
{
	int to_stdout = strcmp(c->out, "-") == 0;
	FILE *f = to_stdout ? stdout : fopen(c->out, "wb");
	int failed = 0;

	if (!f) {
		fprintf(stderr, "Error: could not open %s for writing: %s\n",
				c->out, strerror(errno));
		return 1;
	}
	if (fwrite(c->code, 1, c->len, f) != c->len) failed = 1;
	if (to_stdout && fflush(f) != 0) failed = 1;
	if (!to_stdout && fclose(f) != 0) failed = 1;

	if (failed) {
		fprintf(stderr, "Error: could not write %s\n", c->out);
		if (!to_stdout) remove(c->out);
	}
	return failed;
}

static void mark_written_globals (IR *ir)
{
	for (uint32_t f = 0; f < ir->fn_count; f++) {
		IRFn *fn = ir->fns + f;

		if (f == ir->init_fn) continue;
		for (uint32_t j = 0; j < fn->count; j++) {
			IRInstr *in = ir->instrs + fn->start + j;

			if (in->op == IR_STR_GLOBAL) ir->globals[in->target].written = 1;
		}
	}
}

int codegen_run (CodeGen *c)
{
	mark_written_globals(c->ir);
	if (c->generate(c)) return 1;
	return write_output(c);
}

void cg_printf (CodeGen *c, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (c->cap - c->len < CG_LINE_MAX) {
		c->code = arena_realloc(c->arena, c->code, c->cap, c->cap * 2);
		c->cap *= 2;
	}
	va_start(ap, fmt);
	n = vsnprintf(c->code + c->len, CG_LINE_MAX, fmt, ap);
	va_end(ap);

	if (n < 0) n = 0;
	if (n >= CG_LINE_MAX) n = CG_LINE_MAX - 1;
	c->len += (size_t)n;
}

int cg_is_tail_call (IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx,
		IRInstr *call, uint32_t max_args)
{
	IRInstr *end = ir->instrs + fn->start + fn->count;
	IRInstr *ret = call + 1;
	uint32_t at = (uint32_t)(call - (ir->instrs + fn->start));
	uint8_t from = ir->fns[call->target].ret_type;
	uint8_t to = fn->ret_type;

	while (ret < end && ret->op == IR_LABEL) ret++;
	if (ret >= end || ret->op != IR_RET || call->argc > max_args)
		return 0;
	if (ret->src1 != call->dst) return 0;
	if (call->dst != NO_REG && regs[call->dst].uses != 1) return 0;
	if (ctx->edges[at].fall) return 0;
	return from == to || (types[from].size == 8 && types[to].size == 8);
}

int cg_is_silent (Reg *regs, IRInstr *i)
{
	if (i->op == IR_NOP) return 1;
	if (i->dst == NO_REG) return 0;
	if (regs[i->dst].cstate == VR_CONST || regs[i->dst].cstate == VR_ALIAS
			|| regs[i->dst].cstate == VR_GLOBAL)
		return 1;
	return regs[i->dst].uses == 0 && is_deletable[i->op];
}

