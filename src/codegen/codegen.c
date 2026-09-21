#include "codegen/codegen.h"
#include "codegen/x86_64.h"

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

int init_codegen (CodeGen *codegen, Arch arch, Arena *arena, IR *ir, const char *out)
{
	const Backend *b = backends + arch;

	if (!b->generate) {
		fprintf(stderr, "Error: %s backend is not implemented\n", b->name);
		return 1;
	}
	codegen->arena = arena;
	codegen->ir = ir;
	codegen->out = out;
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

int codegen_run (CodeGen *c)
{
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
