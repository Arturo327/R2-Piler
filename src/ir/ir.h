#ifndef IR_H
#define IR_H

#include <stdint.h>

#include "arena/arena.h"
#include "parser/ast.h"
#include "sema/sema.h"

#define INIT_FN_NAME "__r2_init"
#define NO_REG 0xFFFFFFFF

typedef enum {
	IR_NOP = 0,
	IR_CONST, IR_PARAM,
	IR_LD_GLOBAL, IR_STR_GLOBAL,
	IR_MOVE, IR_EXTEND,
	IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
	IR_AND_A, IR_OR_A, IR_XOR, IR_RS, IR_LS,
	IR_NEG, IR_NOT_A, IR_NOT_L,
	IR_EQ, IR_NE, IR_GT, IR_GE, IR_LT, IR_LE,
	IR_LABEL, IR_JMP, IR_JZ, IR_JNZ,
	IR_ARG, IR_CALL, IR_RET,
	IR_COUNT
} IROp;

typedef struct IRInstr {
	union {
		int64_t imm64;
		uint32_t target;
	};

	uint32_t dst;
	uint32_t src1;
	uint32_t src2;
	uint16_t argc;

	uint8_t op;
	uint8_t data_type;
} IRInstr;

typedef struct IRFn {
	char *name;

	uint32_t start;
	uint32_t count;

	uint32_t reg_count;
	uint32_t param_count;

	uint16_t len;
	uint8_t ret_type;
} IRFn;

typedef struct IRGlobal {
	char *name;
	uint16_t len;
	uint8_t type;
} IRGlobal;

typedef struct IR {
	IRInstr *instrs;
	uint32_t instr_count;
	uint32_t instr_cap;

	IRFn *fns;
	uint32_t fn_count;
	uint32_t fn_cap;

	IRGlobal *globals;
	uint32_t global_count;
	uint32_t global_cap;

	uint32_t init_fn;

	uint32_t *init_order;
	uint32_t init_order_count;

	uint32_t reg_count;
	uint32_t label_count;

	Arena *arena;
	AST *ast;
	SymbolTable *symtab;
} IR;

void ir_init (IR *ir, Arena *arena, Sema *sema);
void ir_gen (IR *ir);
void dump_ir (IR *ir);

#endif
