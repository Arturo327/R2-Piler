#ifndef IR_COMMON_H
#define IR_COMMON_H

#include "ir/opt.h"

void kill_instr (IRInstr *in);
IRInstr *next_real (IRInstr *code, uint32_t from, uint32_t count);
int same_rep (IRFn *fn, uint32_t a, uint32_t b);
int64_t norm_val (uint64_t v, uint8_t type);
int get_srcs (IRInstr *in, uint32_t *out[3]);

void analyze_fn (Optimizer *opt, OptFn *f);
uint32_t label_block (IR *ir, OptFn *f, uint32_t label);
void build_cfg (IR *ir, OptFn *f);
void mark_reachable (OptFn *f);

int reg_const (Optimizer *opt, OptFn *f, uint32_t reg, int64_t *val);
void to_const (OptFn *f, IRInstr *in, int64_t v);
int simplify_instr (Optimizer *opt, OptFn *f, IRInstr *in);

int opt_propagate (Optimizer *opt, OptFn *f);
int opt_fold (Optimizer *opt, OptFn *f);
int opt_cse (Optimizer *opt, OptFn *f);
int opt_coalesce (Optimizer *opt, OptFn *f);
int opt_dce (Optimizer *opt, OptFn *f);
int opt_live_dce (Optimizer *opt, OptFn *f);
int opt_unreachable (Optimizer *opt, OptFn *f);
int opt_jumps (Optimizer *opt, OptFn *f);
int opt_strength (Optimizer *opt, OptFn *f);
int opt_globals (Optimizer *opt, OptFn *f);
int opt_dse (Optimizer *opt, OptFn *f);
int inline_ir (IR *ir, Arena *arena);

void optimize_fn (Optimizer *opt, OptFn *f);
void run_static_init (Optimizer *opt);

#endif
