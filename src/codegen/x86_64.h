#ifndef X86_64_H
#define X86_64_H

#include <stdint.h>
#include "codegen/codegen.h"
#include "codegen/regalloc.h"

typedef struct X86Fn {
	CodeGen *cg;
	IRFn *fn;

	const char *base;

	Reg *regs;
	RegCtx ctx;
	uint32_t *blocks;
	uint64_t *saves;

	uint64_t *frees;

	uint32_t at;
	uint32_t blk;
	uint32_t regsave;
	uint64_t pool;
	uint64_t busy;

	uint32_t outgoing;
	uint32_t frame;
	uint64_t savemask;
	uint8_t leaf;
} X86Fn;

int gen_x86_64 (CodeGen *c);

#endif
