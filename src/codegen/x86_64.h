#ifndef X86_64_H
#define X86_64_H

#include <stdint.h>
#include "codegen/codegen.h"

typedef struct X86Fn {
	CodeGen *cg;
	IRFn *fn;

	uint32_t *slots;
	const char *base;
	uint32_t *uses;
	int64_t *cval;
	uint8_t *cstate;

	uint32_t rax_v;
	uint32_t outgoing;
	uint32_t frame;
	uint8_t leaf;
} X86Fn;

int gen_x86_64 (CodeGen *c);

#endif
