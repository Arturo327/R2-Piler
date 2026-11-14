#include "ir/ir.h"
#include "ir/internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ir/ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t push_instr (IR *ir, IRInstr instr)
{
	if (ir->instr_count >= ir->instr_cap) {
		uint32_t old_cap = ir->instr_cap;
		ir->instr_cap <<= 1;
		ir->instrs = arena_realloc(ir->arena, ir->instrs,old_cap * sizeof(IRInstr),
				ir->instr_cap * sizeof(IRInstr));
	}

	uint32_t idx = ir->instr_count++;
	ir->instrs[idx] = instr;
	return idx;
}

uint32_t push_fn (IR *ir, IRFn fn)
{
	if (ir->fn_count >= ir->fn_cap) {
		uint32_t old_cap = ir->fn_cap;
		ir->fn_cap <<= 1;
		ir->fns = arena_realloc(ir->arena, ir->fns, old_cap * sizeof(IRFn),
				ir->fn_cap * sizeof(IRFn));
	}

	uint32_t idx = ir->fn_count++;
	ir->fns[idx] = fn;
	return idx;
}

static uint32_t push_global (IR *ir, IRGlobal global)
{
	if (ir->global_count >= ir->global_cap) {
		uint32_t old_cap = ir->global_cap;
		ir->global_cap <<= 1;
		ir->globals = arena_realloc(ir->arena, ir->globals, old_cap * sizeof(IRGlobal),
				ir->global_cap * sizeof(IRGlobal));
	}

	uint32_t idx = ir->global_count++;
	ir->globals[idx] = global;
	return idx;
}

static uint32_t push_string (IR *ir, IRString string)
{
	if (ir->string_count >= ir->string_cap) {
		uint32_t old_cap = ir->string_cap;
		ir->string_cap = old_cap ? old_cap << 1 : 8;
		ir->strings = arena_realloc(ir->arena, ir->strings,
				old_cap * sizeof(IRString), ir->string_cap * sizeof(IRString));
	}

	uint32_t idx = ir->string_count++;
	ir->strings[idx] = string;
	return idx;
}

static const IRInstr blank_instr = {
	.dst = NO_REG,
	.src1 = NO_REG,
	.src2 = NO_REG
};

const uint8_t is_deletable[IR_COUNT] =
{
	[IR_CONST] = 1, [IR_ADDR_LOCAL] = 1, [IR_ADDR_GLOBAL] = 1, [IR_STR_LIT] = 1,
	[IR_LD_PTR] = 1, [IR_LD_LOCAL] = 1,
	[IR_MOVE] = 1, [IR_EXTEND] = 1, [IR_SELECT] = 1,
	[IR_ADD] = 1, [IR_SUB] = 1, [IR_MUL] = 1,
	[IR_AND_A] = 1, [IR_OR_A] = 1, [IR_XOR] = 1,
	[IR_RS] = 1, [IR_LS] = 1, [IR_NEG] = 1,
	[IR_NOT_A] = 1, [IR_NOT_L] = 1, [IR_EQ] = 1,
	[IR_NE] = 1, [IR_GT] = 1, [IR_GE] = 1,
	[IR_LT] = 1, [IR_LE] = 1, [IR_LD_GLOBAL] = 1
};

uint32_t new_reg (IR *ir, TypeId type)
{
	uint32_t r = ir->reg_count++;

	if (r >= ir->reg_type_cap) {
		uint32_t old = ir->reg_type_cap;
		ir->reg_type_cap = old ? old << 1 : 64;
		ir->reg_types = arena_realloc(ir->arena, ir->reg_types,
				old * sizeof(TypeId), ir->reg_type_cap * sizeof(TypeId));
	}
	if (r >= ir->addressed_cap) {
		uint32_t old = ir->addressed_cap;
		ir->addressed_cap = old ? old << 1 : 64;
		ir->addressed = arena_realloc(ir->arena, ir->addressed,
				old * sizeof(uint8_t), ir->addressed_cap * sizeof(uint8_t));
	}
	ir->reg_types[r] = type;
	ir->addressed[r] = 0;
	return r;
}

static TypeId result_type (uint8_t op, TypeId type)
{
	if (op >= IR_EQ && op <= IR_LE) return TYPE_i64;
	if (op == IR_NOT_L) return TYPE_i64;
	return type;
}

void ir_init (IR *ir, Arena *arena, Sema *sema, int use_select)
{
	ir->arena = arena;
	ir->ast = sema->ast;
	ir->symtab = &sema->table;
	ir->init_order = sema->init_order;
	ir->init_order_count = sema->init_order_count;
	ir->statics = sema->statics;
	ir->static_count = sema->static_count;

	ir->init_fn = 0;
	ir->reg_count = 0;
	ir->label_count = 0;
	ir->loops = NULL;
	ir->loop_count = 0;
	ir->loop_cap = 0;
	ir->use_select = use_select;
	ir->no_tail = 0;
	ir->scalarize = 0;
	ir->init_dirty = 0;
	ir->sret = NO_REG;
	ir->nrvo = NO_SYMBOL;
	ir->ret_node = NO_NODE;
	ir->temps = NULL;
	ir->temp_count = 0;
	ir->temp_cap = 0;
	ir->desc = arena_alloc(arena, sizeof(uint32_t) * (sema->ast->count ? sema->ast->count : 1));

	ir->reg_types = NULL;
	ir->reg_type_cap = 0;
	ir->addressed = NULL;
	ir->addressed_cap = 0;

	ir->instr_count = 0;
	ir->instr_cap = 256;
	ir->instrs = arena_alloc(arena, sizeof(IRInstr) * ir->instr_cap);

	ir->fn_count = 0;
	ir->fn_cap = 16;
	ir->fns = arena_alloc(arena, sizeof(IRFn) * ir->fn_cap);

	ir->global_count = 0;
	ir->global_cap = 16;
	ir->globals = arena_alloc(arena, sizeof(IRGlobal) * ir->global_cap);

	ir->string_count = 0;
	ir->string_cap = 0;
	ir->strings = NULL;

	ir->string_tab = NULL;
	ir->string_tab_cap = 0;
}

uint32_t ret_count (IR *ir, uint32_t ret)
{
	uint32_t count = 1;

	for (uint32_t item = ir->ast->nodes[ret].child; item != NO_NODE;
			item = ir->ast->nodes[item].next_bro)
		count++;
	return count;
}

uint32_t ret_node_at (IR *ir, uint32_t ret, uint32_t i)
{
	uint32_t item = ir->ast->nodes[ret].child;

	if (i == 0) return ret;
	while (i-- > 1 && item != NO_NODE)
		item = ir->ast->nodes[item].next_bro;
	return item;
}

int fn_uses_sret (IR *ir, uint32_t ret)
{
	TypeId t = ir->ast->nodes[ret].data_type;

	if (ir->ast->nodes[ret].child != NO_NODE) return 1;
	if (type_dyn_rank(t) > 0) return 1;
	return type_kind(t) == TK_STRUCT && !struct_eightbytes(t);
}

TypeId ret_slot_type (TypeId t)
{
	if (type_kind(t) == TK_ARRAY && type_dyn_rank(t) == 0)
		return type_ptr_to(type_flat_element(t));
	return t;
}

uint32_t ret_off (IR *ir, uint32_t ret, uint32_t i)
{
	uint32_t off = 0;

	for (uint32_t k = 0; k < i; k++) {
		uint32_t size = types[ret_slot_type(ir->ast->nodes[ret_node_at(ir, ret, k)].data_type)].size;

		off += (size + 7u) & ~7u;
	}
	return off;
}

TypeId ret_buf_type (IR *ir, uint32_t ret)
{
	if (ret_count(ir, ret) > 1)
		return type_array_of(TYPE_u64, ret_off(ir, ret, ret_count(ir, ret)) / 8);
	return ret_slot_type(ir->ast->nodes[ret].data_type);
}

uint32_t call_ret_node (IR *ir, ASTNode *call)
{
	if (call->sym == NO_SYMBOL) return NO_NODE;

	uint32_t decl = ir->symtab->symbols[call->sym].decl;
	if (decl == NO_NODE) return NO_NODE;
	return ir->ast->nodes[ir->ast->nodes[decl].child].next_bro;
}

static uint32_t param_slots (IR *ir, ASTNode *n)
{
	uint32_t ret = ir->ast->nodes[n->child].next_bro;
	uint32_t count = (uint32_t)fn_uses_sret(ir, ret);
	uint32_t p = ir->ast->nodes[n->child].child;

	while (p != NO_NODE) {
		TypeId t = ir->ast->nodes[p].data_type;

		count += struct_eightbytes(t) ? struct_eightbytes(t)
				: 1u + (uint32_t)type_dyn_rank(t);
		p = ir->ast->nodes[p].next_bro;
	}
	return count;
}

static uint32_t declare_fn (IR *ir, ASTNode *n, Symbol *sym)
{
	IRFn fn = {0};
	uint32_t ret = ir->ast->nodes[n->child].next_bro;

	fn.name = n->str;
	fn.len = n->len;
	if (fn_uses_sret(ir, ret)) fn.ret_type = TYPE_VOID;
	else if (type_kind(sym->type) == TK_ARRAY)
		fn.ret_type = type_ptr_to(type_flat_element(sym->type));
	else if (struct_eightbytes(sym->type)) fn.ret_type = TYPE_u64;
	else fn.ret_type = sym->type;
	fn.param_count = param_slots(ir, n);
	fn.is_extern = n->type == NODE_EXTERN_FN_DEC;
	return push_fn(ir, fn);
}

static uint32_t declare_global (IR *ir, ASTNode *n, Symbol *sym)
{
	IRGlobal global = {0};
	global.name = n->str;
	global.len = n->len;
	global.type = sym->type;
	global.is_extern = n->type == NODE_EXTERN_VAR_DEC;
	return push_global(ir, global);
}

static uint32_t declare_static (IR *ir, uint32_t node_idx, uint32_t k)
{
	ASTNode *n = ir->ast->nodes + node_idx;
	Symbol *sym = ir->symtab->symbols + n->sym;
	size_t cap = (size_t)n->len + 24;
	char *name = arena_alloc(ir->arena, cap);
	int len = snprintf(name, cap, "sv.%u.%.*s", k, (int)n->len, n->str);
	IRGlobal global = {0};

	global.name = name;
	global.len = (uint16_t)len;
	global.type = sym->type;
	return push_global(ir, global);
}

void declare_globals (IR *ir)
{
	uint32_t idx = ir->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = ir->ast->nodes + idx;

		if (n->type != NODE_FN_DEC && n->type != NODE_VAR_DEC && n->type != NODE_ARR_DEC
				&& n->type != NODE_EXTERN_FN_DEC && n->type != NODE_EXTERN_VAR_DEC) {
			idx = n->next_bro;
			continue;
		}

		Symbol *s = ir->symtab->symbols + n->sym;
		if (n->type == NODE_FN_DEC || n->type == NODE_EXTERN_FN_DEC)
			s->ir_id = declare_fn(ir, n, s);
		else s->ir_id = declare_global(ir, n, s);

		idx = n->next_bro;
	}

	for (uint32_t i = 0; i < ir->static_count; i++) {
		uint32_t node = ir->statics[i];
		Symbol *s = ir->symtab->symbols + ir->ast->nodes[node].sym;
		s->ir_id = declare_static(ir, node, i);
	}
}

void make_arg_dec (IR *ir, uint32_t dst, uint32_t idx, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_PARAM;
	i.dst = dst;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
}

void make_arg (IR *ir, uint32_t val, uint32_t idx)
{
	IRInstr i = blank_instr;
	i.op = IR_ARG;
	i.src1 = val;
	i.target = idx;
	push_instr(ir, i);
}

void make_const_into (IR *ir, uint32_t dst, int64_t val, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_CONST;
	i.dst = dst;
	i.imm64 = val;
	i.data_type = type;
	push_instr(ir, i);
}

uint32_t make_const (IR *ir, int64_t val, TypeId type)
{
	uint32_t dst = new_reg(ir, type);
	make_const_into(ir, dst, val, type);
	return dst;
}

uint32_t make_ld_global (IR *ir, uint32_t idx, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_LD_GLOBAL;
	i.dst = new_reg(ir, type);
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
	return i.dst;
}

void make_str_global (IR *ir, uint32_t idx, uint32_t val, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_STR_GLOBAL;
	i.src1 = val;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
}

void make_addr_local (IR *ir, uint32_t dst, uint32_t var, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_ADDR_LOCAL;
	i.dst = dst;
	i.src1 = var;
	i.data_type = type;
	push_instr(ir, i);
}

void make_addr_global (IR *ir, uint32_t dst, uint32_t global, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_ADDR_GLOBAL;
	i.dst = dst;
	i.target = global;
	i.data_type = type;
	push_instr(ir, i);
}

uint32_t make_ld_ptr (IR *ir, uint32_t ptr, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_LD_PTR;
	i.dst = new_reg(ir, type);
	i.src1 = ptr;
	i.data_type = type;
	push_instr(ir, i);
	return i.dst;
}

void make_str_ptr (IR *ir, uint32_t ptr, uint32_t val, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_STR_PTR;
	i.src1 = ptr;
	i.src2 = val;
	i.data_type = type;
	push_instr(ir, i);
}

static uint32_t fnv_string (const char *data, uint32_t len)
{
	uint32_t h = 2166136261u;

	for (uint32_t i = 0; i < len; i++)
		h = (h ^ (uint8_t)data[i]) * 16777619u;
	h ^= h >> 15;
	return h;
}

static void string_tab_grow (IR *ir, uint32_t new_cap)
{
	uint32_t *tab = arena_alloc(ir->arena, (size_t)new_cap * sizeof(uint32_t));
	memset(tab, 0, (size_t)new_cap * sizeof(uint32_t));

	for (uint32_t i = 0; i < ir->string_count; i++) {
		IRString *s = ir->strings + i;
		uint32_t h = fnv_string(s->data, s->len) & (new_cap - 1);

		while (tab[h]) h = (h + 1) & (new_cap - 1);
		tab[h] = i + 1;
	}

	ir->string_tab = tab;
	ir->string_tab_cap = new_cap;
}

static uint32_t intern_string (IR *ir, ASTNode *n)
{
	if (ir->string_tab_cap == 0 || ir->string_count * 2 >= ir->string_tab_cap) {
		uint32_t new_cap = ir->string_tab_cap ? ir->string_tab_cap << 1 : 16;

		string_tab_grow(ir, new_cap);
	}

	uint32_t cap = ir->string_tab_cap;
	uint32_t h = fnv_string(n->str, n->len) & (cap - 1);

	while (ir->string_tab[h]) {
		uint32_t idx = ir->string_tab[h] - 1;
		IRString *s = ir->strings + idx;

		if (s->len == n->len && !memcmp(s->data, n->str, n->len)) return idx;
		h = (h + 1) & (cap - 1);
	}

	IRString string;
	string.data = arena_alloc(ir->arena, (size_t)n->len + 1);
	memcpy(string.data, n->str, n->len);
	string.data[n->len] = '\0';
	string.len = n->len;

	uint32_t idx = push_string(ir, string);

	h = fnv_string(n->str, n->len) & (ir->string_tab_cap - 1);
	while (ir->string_tab[h]) h = (h + 1) & (ir->string_tab_cap - 1);
	ir->string_tab[h] = idx + 1;
	return idx;
}

uint32_t make_str_lit (IR *ir, ASTNode *n)
{
	IRInstr i = blank_instr;
	i.op = IR_STR_LIT;
	i.dst = new_reg(ir, n->data_type);
	i.target = intern_string(ir, n);
	i.data_type = n->data_type;
	push_instr(ir, i);
	return i.dst;
}

uint32_t make_blob (IR *ir, const uint8_t *data, uint32_t len)
{
	IRInstr i = blank_instr;
	IRString string;

	string.data = arena_alloc(ir->arena, (size_t)len + 1);
	memcpy(string.data, data, len);
	string.data[len] = '\0';
	string.len = len;

	i.op = IR_STR_LIT;
	i.dst = new_reg(ir, type_ptr_to(TYPE_u8));
	i.target = push_string(ir, string);
	i.data_type = ir->reg_types[i.dst];
	push_instr(ir, i);
	return i.dst;
}

uint32_t make_call (IR *ir, uint32_t idx, uint16_t argc, TypeId ret_type)
{
	IRInstr i = blank_instr;
	i.op = IR_CALL;
	i.dst = ret_type == TYPE_VOID ? NO_REG : new_reg(ir, ret_type);
	i.target = idx;
	i.argc = argc;
	i.data_type = ret_type;
	push_instr(ir, i);
	return i.dst;
}

void make_move (IR *ir, uint32_t dst, uint32_t src, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_MOVE;
	i.dst = dst;
	i.src1 = src;
	i.data_type = type;
	push_instr(ir, i);
}

uint32_t make_op (IR *ir, uint8_t op, uint32_t a, uint32_t b, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = op;
	i.dst = new_reg(ir, result_type(op, type));
	i.src1 = a;
	i.src2 = b;
	i.data_type = type;
	push_instr(ir, i);
	return i.dst;
}

void make_jump (IR *ir, uint8_t op, uint32_t cond, uint32_t label)
{
	IRInstr i = blank_instr;
	i.op = op;
	i.src1 = cond;
	i.target = label;
	push_instr(ir, i);
}

void make_label (IR *ir, uint32_t label)
{
	IRInstr i = blank_instr;
	i.op = IR_LABEL;
	i.target = label;
	push_instr(ir, i);
}

void make_ret (IR *ir, uint32_t val, TypeId type)
{
	IRInstr i = blank_instr;
	i.op = IR_RET;
	i.src1 = val;
	i.data_type = type;
	push_instr(ir, i);
}

void make_ret2 (IR *ir, uint32_t lo, uint32_t hi, TypeId type)
{
	IRInstr i = blank_instr;

	i.op = IR_RET;
	i.src1 = lo;
	i.src2 = hi;
	i.data_type = type;
	push_instr(ir, i);
}

uint32_t make_rethi (IR *ir)
{
	IRInstr i = blank_instr;

	i.op = IR_RETHI;
	i.dst = new_reg(ir, TYPE_u64);
	i.data_type = TYPE_u64;
	push_instr(ir, i);
	return i.dst;
}

void make_select (IR *ir, uint32_t dst, uint32_t a,
		uint32_t b, uint32_t c, TypeId type)
{
	IRInstr i = blank_instr;

	i.op = IR_SELECT;
	i.dst = dst;
	i.src1 = a;
	i.src2 = b;
	i.cond = c;
	i.data_type = type;
	push_instr(ir, i);
}
