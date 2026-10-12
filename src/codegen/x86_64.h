#ifndef X86_64_H
#define X86_64_H

#include <stdint.h>
#include "codegen/codegen.h"
#include "codegen/regalloc.h"

enum {
	VR_NONE = 0,
	VR_CONST = 1,
	VR_MEM = 2,
	VR_ALIAS = 3,
	VR_REG = 4
};

typedef struct X86Fn {
	CodeGen *cg;
	IRFn *fn;

	const char *base;

	Reg *regs;
	RegCtx ctx;
	int8_t *homes;
	uint32_t *blocks;
	uint16_t *saves;

	uint32_t rax_v;
	uint32_t outgoing;
	uint32_t frame;
	uint8_t leaf;
} X86Fn;

int gen_x86_64 (CodeGen *c);

#endif
