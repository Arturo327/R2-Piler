#include "ir/opts/common.h"

void kill_instr (IRInstr *in)
{
	in->op = IR_NOP;
	in->dst = NO_REG;
	in->src1 = NO_REG;
	in->src2 = NO_REG;
	in->argc = 0;
}

IRInstr *next_real (IRInstr *code, uint32_t from, uint32_t count)
{
	while (from < count && code[from].op == IR_NOP) from++;
	return from < count ? code + from : NULL;
}

int64_t norm_val (uint64_t v, uint8_t type)
{
	const Type *t = types + type;
	unsigned sh = 64u - t->size * 8u;

	v <<= sh;
	return t->sign ? (int64_t)v >> sh : (int64_t)(v >> sh);
}
