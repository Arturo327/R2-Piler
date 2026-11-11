#ifndef IR_INTERNAL_H
#define IR_INTERNAL_H

#include "ir/ir.h"

uint32_t push_fn (IR *ir, IRFn fn);
uint32_t new_reg (IR *ir, TypeId type);

uint32_t agg_root_sym (AST *ast, uint32_t idx);
uint32_t ret_count (IR *ir, uint32_t ret);
uint32_t ret_node_at (IR *ir, uint32_t ret, uint32_t i);
uint32_t ret_off (IR *ir, uint32_t ret, uint32_t i);
int fn_uses_sret (IR *ir, uint32_t ret);
TypeId ret_buf_type (IR *ir, uint32_t ret);
uint32_t call_ret_node (IR *ir, ASTNode *call);
TypeId ret_slot_type (TypeId t);
uint32_t add_offset (IR *ir, uint32_t base, uint64_t off);
void gen_multi_assign (IR *ir, ASTNode *n);
int struct_is_small (TypeId type);

void ir_scan_structs (IR *ir);
void make_arg_dec (IR *ir, uint32_t dst, uint32_t idx, TypeId type);
void declare_globals (IR *ir);
void make_arg (IR *ir, uint32_t val, uint32_t idx);
void make_const_into (IR *ir, uint32_t dst, int64_t val, TypeId type);
uint32_t make_const (IR *ir, int64_t val, TypeId type);
uint32_t make_ld_global (IR *ir, uint32_t idx, TypeId type);
void make_str_global (IR *ir, uint32_t idx, uint32_t val, TypeId type);
void make_addr_local (IR *ir, uint32_t dst, uint32_t var, TypeId type);
void make_addr_global (IR *ir, uint32_t dst, uint32_t global, TypeId type);
uint32_t make_ld_ptr (IR *ir, uint32_t ptr, TypeId type);
void make_str_ptr (IR *ir, uint32_t ptr, uint32_t val, TypeId type);
uint32_t make_str_lit (IR *ir, ASTNode *n);
uint32_t make_call (IR *ir, uint32_t idx, uint16_t argc, TypeId ret_type);
void make_move (IR *ir, uint32_t dst, uint32_t src, TypeId type);
uint32_t make_op (IR *ir, uint8_t op, uint32_t a, uint32_t b, TypeId type);
void make_jump (IR *ir, uint8_t op, uint32_t cond, uint32_t label);
void make_label (IR *ir, uint32_t label);
void make_ret (IR *ir, uint32_t val, TypeId type);
void make_select (IR *ir, uint32_t dst, uint32_t a, uint32_t b, uint32_t c, TypeId type);

uint32_t gen_expr (IR *ir, ASTNode *n);
void gen_var_dec (IR *ir, ASTNode *n);
void gen_jump_if (IR *ir, ASTNode *n, uint32_t label, int when);
void gen_array_init (IR *ir, uint32_t base, uint32_t init_idx, TypeId arr, int zero);
void gen_struct_into (IR *ir, uint32_t dst, uint32_t expr, TypeId type, int fresh);
uint32_t gen_slice_desc (IR *ir, uint32_t dst, ASTNode *val, TypeId type);
uint32_t local_storage_addr (IR *ir, Symbol *s, TypeId type);
int gen_select (IR *ir, ASTNode *n);
void gen_inc_dec (IR *ir, ASTNode *n);

#endif
