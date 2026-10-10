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

int same_rep (IRFn *fn, uint32_t a, uint32_t b)
{
	const Type *ta = types + fn->reg_types[a];
	const Type *tb = types + fn->reg_types[b];

	return ta->size == tb->size && (ta->size == 8 || ta->sign == tb->sign);
}

int64_t norm_val (uint64_t v, uint8_t type)
{
	const Type *t = types + type;
	unsigned sh = 64u - t->size * 8u;

	v <<= sh;
	return t->sign ? (int64_t)v >> sh : (int64_t)(v >> sh);
}
