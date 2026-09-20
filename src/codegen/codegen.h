#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>

#include "arena/arena.h"
#include "ir/ir.h"

typedef enum {
	ARCH_X86_64 = 0,
	ARCH_ARM,
	ARCH_RISCV,
	ARCH_INTERP,
	ARCH_COUNT
} Arch;

typedef struct CodeGen CodeGen;

struct CodeGen {
	IR *ir;
	Arena *arena;
	const char *out;

	char *code;
	size_t len;
	size_t cap;

	int (*generate) (CodeGen *c);
};

int init_codegen (CodeGen *codegen, Arch arch, Arena *arena, IR *ir, const char *out);
int codegen_run (CodeGen *codegen);
void cg_printf (CodeGen *c, const char *fmt, ...);

#endif
