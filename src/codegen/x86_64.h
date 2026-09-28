#ifndef X86_64_H
#define X86_64_H

#include <stdint.h>
#include "codegen/codegen.h"

typedef struct X86Fn {
	CodeGen *cg;
	IRFn *fn;

	uint32_t *slots;
	uint32_t outgoing;
	uint32_t frame;
} X86Fn;

int gen_x86_64 (CodeGen *c);

#endif
