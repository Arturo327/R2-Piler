#ifndef X86_64_COMMON_H
#define X86_64_COMMON_H

#include <stdint.h>

#include "codegen/x86-64/x86_64.h"
#include "codegen/regalloc.h"
#include "codegen/parallel.h"
#include "ir/opts/common.h"

#define SYM_PREFIX "__r2_"
#define INIT_SYM "__r2.init"

#define RED_ZONE 128
#define X86_REG_NUM 14
#define X86_CALLEE_MASK 0x3E00ull
#define X86_CALLER_HOME_MASK 0x180ull
#define ARG_REGS_MASK 0x7Eull

enum {
	RAX = 0,
	RCX = 1,
	RDX = 2
};

typedef struct X86Fn {
	CodeGen *cg;
	IRFn *fn;

	const char *base;

	Reg *regs;
	RegCtx ctx;
	uint32_t *blocks;
	EdgeMoves *edges;
	uint64_t *saves;

	PMove *moves;
	PMAction *acts;

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

extern const char *x86_regs_names[];
extern const char x86_mem_suf[9];
extern const uint8_t x86_arg_regs[6];

static inline const char *reg_name (uint8_t reg, uint8_t size)
{
	uint8_t idx = size == 1 ? 0 : size == 2 ? 1 : size == 4 ? 2
			: size == 8 ? 3 : 0;

	return x86_regs_names[(idx << 4) + reg];
}

int x86_is_silent (X86Fn *f, IRInstr *i);
IRInstr *x86_next_live (X86Fn *f, IRInstr *i, IRInstr *end);
void x86_layout_fn (X86Fn *f);

void x86_move_saved (X86Fn *f, uint64_t mask, int restore);
void x86_begin_instr (X86Fn *f, IRInstr *i);
int x86_phys_reg (X86Fn *f, uint32_t vreg);
uint64_t x86_home_bit (X86Fn *f, uint32_t vreg);
uint8_t x86_grab (X86Fn *f);
void x86_take_fixed (X86Fn *f, uint8_t r, uint32_t keeps);
uint8_t x86_work_reg (X86Fn *f, uint32_t dst, uint64_t avoid);
void x86_check_regalloc (X86Fn *f);
void x86_check_arg_group (X86Fn *f, IRInstr *first, IRInstr *end);

const char *x86_load_op (uint8_t t, uint8_t *size);
const char *x86_load_op_bits (uint8_t size, uint8_t sign, uint8_t *w);
int64_t x86_const_ext (X86Fn *f, uint32_t vreg);
void x86_load_imm (X86Fn *f, uint8_t reg, int64_t v);
void x86_load_reg (X86Fn *f, uint8_t reg, uint32_t vreg);
void x86_store_reg (X86Fn *f, uint8_t reg, uint32_t vreg);
void x86_store_imm (X86Fn *f, uint32_t vreg, int64_t v);
void x86_op_reg (X86Fn *f, const char *op, uint8_t w, uint32_t vreg);

void x86_make_globals (CodeGen *c);
void x86_make_ld_global (X86Fn *f, IRInstr *i);
void x86_make_str_global (X86Fn *f, IRInstr *i);

void x86_make_edge (X86Fn *f, uint32_t at, int taken);
void x86_make_stubs (X86Fn *f);

void x86_parallel_move (X86Fn *f, PMove *moves, uint32_t count);
int x86_make_param (X86Fn *f, IRInstr *i);
void x86_make_move (X86Fn *f, IRInstr *i);

uint64_t x86_shift_amt (uint64_t v);
void x86_make_arith (X86Fn *f, IRInstr *i);
void x86_make_shift (X86Fn *f, IRInstr *i);
void x86_make_unary (X86Fn *f, IRInstr *i, const char *op);
void x86_make_not_l (X86Fn *f, IRInstr *i);

void x86_make_divmod (X86Fn *f, IRInstr *i);

int x86_make_cmp_instr (X86Fn *f, IRInstr *i);
void x86_make_select (X86Fn *f, IRInstr *i);
int x86_const_jump (X86Fn *f, IRInstr *i);
void x86_make_jump (X86Fn *f, IRInstr *i);

IRInstr *x86_tail_ret (IRInstr *call);
int x86_tail_match (X86Fn *f, IRInstr *call, IRInstr *ret);
int x86_make_tail_call (X86Fn *f, IRInstr *i);
int x86_make_arg (X86Fn *f, IRInstr *i);
void x86_make_call (X86Fn *f, IRInstr *i);
void x86_make_ret (X86Fn *f, IRInstr *i);

int x86_make_rmw (X86Fn *f, IRInstr *a);
int x86_try_rmw_inplace (X86Fn *f, IRInstr *i);

void x86_print_fn_name (CodeGen *c, IRFn *fn);
void x86_make_fn (X86Fn *f);

#endif
