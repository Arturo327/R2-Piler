#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>

#include "arena/arena.h"
#include "ir/ir.h"
#include "ir/opt.h"
#include "codegen/regalloc/regalloc.h"

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

	OptLevel opt_level;
	int (*generate) (CodeGen *c);
};

int init_codegen (CodeGen *codegen, Arch arch, Arena *arena, IR *ir, const char *out, OptLevel level);
int codegen_run (CodeGen *codegen);
void cg_printf (CodeGen *c, const char *fmt, ...);

int cg_is_tail_call (IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, IRInstr *call, uint32_t max_args);
int cg_is_silent (Reg *regs, IRInstr *i);

#endif
