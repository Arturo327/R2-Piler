#include "ir/ir.h"
#include "ir/internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void gen_stmt (IR *ir, ASTNode *n);
static void gen_block (IR *ir, ASTNode *parent);

static void gen_var_dec (IR *ir, ASTNode *n)
{
	if (n->flags & NODE_FLAG_STATIC) return;

	Symbol *s = ir->symtab->symbols + n->sym;
	uint32_t reg;

	if (type_dyn_rank(s->type) > 0 || type_kind(s->type) == TK_SLICE) {
		reg = new_reg(ir, s->type);
		s->ir_id = reg;
		ir->addressed[reg] = 1;
		ir->no_tail = 1;
		if (n->child != NO_NODE)
			gen_slice_desc(ir, local_storage_addr(ir, s, s->type),
					ir->ast->nodes + n->child, s->type);
		return;
	}
	if (type_kind(s->type) == TK_STRUCT) {
		reg = new_reg(ir, s->type);
		s->ir_id = reg;
		ir->addressed[reg] = 1;
		ir->no_tail = 1;
		if (n->child != NO_NODE)
			gen_struct_into(ir, local_storage_addr(ir, s, s->type),
					n->child, s->type, 1);
		return;
	}
	if (type_kind(s->type) == TK_ARRAY) {
		reg = new_reg(ir, s->type);
		s->ir_id = reg;
		ir->addressed[reg] = 1;
		if (n->child != NO_NODE)
			gen_array_init(ir, local_storage_addr(ir, s, s->type),
					n->child, s->type, 1);
		return;
	}

	if (n->child == NO_NODE) reg = new_reg(ir, s->type);
	else reg = gen_expr(ir, ir->ast->nodes + n->child);

	s->ir_id = reg;
	if (s->addressed) ir->addressed[reg] = 1;
}

static void gen_return (IR *ir, ASTNode *n)
{
	uint32_t reg = NO_REG;
	TypeId type = TYPE_VOID;

	if (n->child != NO_NODE) {
		ASTNode *val = ir->ast->nodes + n->child;

		if (ir->sret != NO_REG) {
			gen_struct_into(ir, ir->sret, n->child, val->data_type, 1);
			make_ret(ir, NO_REG, TYPE_VOID);
			return;
		}
		reg = gen_expr(ir, val);
		type = val->data_type;
	}
	make_ret(ir, reg, type);
}

static void gen_branch (IR *ir, uint32_t cond, uint32_t body, uint32_t end, int more)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t next = more ? ir->label_count++ : end;

	gen_jump_if(ir, nodes + cond, next, 0);
	gen_stmt(ir, nodes + body);
	if (!more) return;
	make_jump(ir, IR_JMP, NO_REG, end);
	make_label(ir, next);
}

static void gen_if (IR *ir, ASTNode *n)
{
	if (ir->use_select && gen_select(ir, n)) return;

	ASTNode *nodes = ir->ast->nodes;
	uint32_t cond = n->child;
	uint32_t body = nodes[cond].next_bro;
	uint32_t branch = nodes[body].next_bro;
	uint32_t end = ir->label_count++;

	gen_branch(ir, cond, body, end, branch != NO_NODE);
	while (branch != NO_NODE) {
		ASTNode *b = nodes + branch;
		int more = b->next_bro != NO_NODE;

		if (b->type == NODE_ELSE) gen_stmt(ir, nodes + b->child);
		else gen_branch(ir, b->child, nodes[b->child].next_bro, end, more);
		branch = b->next_bro;
	}
	make_label(ir, end);
}

static void gen_while (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t body = nodes[n->child].next_bro;
	uint32_t l_body = ir->label_count++;
	uint32_t l_cond = ir->label_count++;

	make_jump(ir, IR_JMP, NO_REG, l_cond);
	make_label(ir, l_body);
	gen_stmt(ir, nodes + body);
	make_label(ir, l_cond);
	gen_jump_if(ir, nodes + n->child, l_body, 1);
}

static void gen_for (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	ASTNode *cond = nodes + nodes[n->child].next_bro;
	ASTNode *updt = nodes + cond->next_bro;
	ASTNode *body = nodes + updt->next_bro;
	uint32_t l_body = ir->label_count++;
	uint32_t l_cond = ir->label_count++;

	gen_stmt(ir, nodes + n->child);
	if (cond->type != NODE_EMPTY)
		make_jump(ir, IR_JMP, NO_REG, l_cond);

	make_label(ir, l_body);
	gen_stmt(ir, body);
	gen_stmt(ir, updt);
	make_label(ir, l_cond);
	if (cond->type == NODE_EMPTY) make_jump(ir, IR_JMP, NO_REG, l_body);
	else gen_jump_if(ir, cond, l_body, 1);
}

static uint32_t ir_case_body (IR *ir, uint32_t clause)
{
	uint32_t v = ir->ast->nodes[clause].child;
	while (v != NO_NODE && ir->ast->nodes[v].type != NODE_BLOCK)
		v = ir->ast->nodes[v].next_bro;
	return v;
}

static void gen_switch_tests (IR *ir, uint32_t cond, uint32_t scrut,
		TypeId type, uint32_t *labels, uint32_t dflt)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t clause = nodes[cond].next_bro;
	uint32_t k = 0;

	while (clause != NO_NODE) {
		ASTNode *c = nodes + clause;

		if (c->type != NODE_DEFAULT) {
			uint32_t v = c->child;

			while (v != NO_NODE && nodes[v].type != NODE_BLOCK) {
				uint32_t eq = make_op(ir, IR_EQ, scrut,
						gen_expr(ir, nodes + v), type);

				make_jump(ir, IR_JNZ, eq, labels[k]);
				v = nodes[v].next_bro;
			}
		}
		k++;
		clause = nodes[clause].next_bro;
	}
	make_jump(ir, IR_JMP, NO_REG, dflt);
}

static void gen_switch (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t cond = n->child;
	TypeId type = nodes[cond].data_type;
	uint32_t scrut = gen_expr(ir, nodes + cond);
	uint32_t end = ir->label_count++;

	uint32_t clause = nodes[cond].next_bro;
	uint32_t count = 0;
	while (clause != NO_NODE) {
		count++;
		clause = nodes[clause].next_bro;
	}

	uint32_t labels[count + 1];

	clause = nodes[cond].next_bro;
	count = 0;
	uint32_t dflt = 0;

	while (clause != NO_NODE) {
		labels[count] = ir->label_count++;
		if (nodes[clause].type == NODE_DEFAULT) dflt = labels[count];
		count++;
		clause = nodes[clause].next_bro;
	}

	gen_switch_tests(ir, cond, scrut, type, labels, dflt);

	count = 0;
	clause = nodes[cond].next_bro;
	while (clause != NO_NODE) {
		make_label(ir, labels[count++]);
		gen_stmt(ir, nodes + ir_case_body(ir, clause));
		make_jump(ir, IR_JMP, NO_REG, end);
		clause = nodes[clause].next_bro;
	}
	make_label(ir, end);
}

static void gen_stmt (IR *ir, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_VAR_DEC: gen_var_dec(ir, n); return;
	case NODE_ARR_DEC: gen_var_dec(ir, n); return;
	case NODE_BLOCK: gen_block(ir, n); return;
	case NODE_RET: gen_return(ir, n); return;
	case NODE_WHILE: gen_while(ir, n); return;
	case NODE_FOR: gen_for(ir, n); return;
	case NODE_IF: gen_if(ir, n); return;
	case NODE_SWITCH: gen_switch(ir, n); return;
	case NODE_INC: case NODE_DEC: gen_inc_dec(ir, n); return;
	case NODE_EMPTY: return;
	default: gen_expr(ir, n); return;
	}
}

static void gen_block (IR *ir, ASTNode *parent)
{
	uint32_t stmt = parent->child;
	while (stmt != NO_NODE) {
		ASTNode *n = ir->ast->nodes + stmt;
		gen_stmt(ir, n);
		stmt = n->next_bro;
	}
}

static uint32_t dec_aggregate_param (IR *ir, Symbol *s, uint32_t idx)
{
	TypeId ptr = type_ptr_to(type_flat_element(s->type));
	int dyn = type_dyn_rank(s->type);

	s->ir_id = new_reg(ir, ptr);
	s->ir_dims = arena_alloc(ir->arena, (size_t)dyn * sizeof(uint32_t));
	make_arg_dec(ir, s->ir_id, idx++, ptr);
	for (int d = 0; d < dyn; d++) {
		s->ir_dims[d] = new_reg(ir, TYPE_i64);
		make_arg_dec(ir, s->ir_dims[d], idx++, TYPE_i64);
	}
	return idx;
}

static void gen_args_dec (IR *ir, uint32_t node)
{
	uint32_t p = ir->ast->nodes[node].child;
	uint32_t idx = ir->sret == NO_REG ? 0 : 1;

	while (p != NO_NODE) {
		ASTNode *n = ir->ast->nodes + p;
		Symbol *s = ir->symtab->symbols + n->sym;
		TypeKind kind = type_kind(s->type);

		if (kind == TK_ARRAY || kind == TK_SLICE || kind == TK_STRUCT) {
			idx = dec_aggregate_param(ir, s, idx);
		} else {
			s->ir_id = new_reg(ir, s->type);
			if (s->addressed) ir->addressed[s->ir_id] = 1;
			make_arg_dec(ir, s->ir_id, idx++, s->type);
		}
		p = n->next_bro;
	}
}

static void fn_end (IR *ir, uint32_t idx)
{
	IRFn *fn = ir->fns + idx;

	make_ret(ir, NO_REG, TYPE_VOID);
	fn->count = ir->instr_count - fn->start;
	fn->reg_count = ir->reg_count;
	fn->reg_types = ir->reg_types;
	fn->addressed = ir->addressed;
	fn->no_tail = ir->no_tail;
}

static void gen_fn (IR *ir, ASTNode *n, uint32_t fn_idx)
{
	uint32_t args = n->child;
	uint32_t ret = ir->ast->nodes[args].next_bro;
	uint32_t body = ir->ast->nodes[ret].next_bro;
	TypeId ret_type = ir->ast->nodes[ret].data_type;

	ir->reg_count = 0;
	ir->reg_types = NULL;
	ir->reg_type_cap = 0;
	ir->addressed = NULL;
	ir->addressed_cap = 0;
	ir->no_tail = 0;
	ir->sret = NO_REG;

	ir->fns[fn_idx].start = ir->instr_count;
	if (type_kind(ret_type) == TK_STRUCT) {
		TypeId ptr = type_ptr_to(ret_type);

		ir->sret = new_reg(ir, ptr);
		make_arg_dec(ir, ir->sret, 0, ptr);
	}
	gen_args_dec(ir, args);
	gen_block(ir, ir->ast->nodes + body);
	fn_end(ir, fn_idx);
}

static void gen_var_init (IR *ir, uint32_t idx)
{
	ASTNode *n = ir->ast->nodes + idx;
	Symbol *s = ir->symtab->symbols + n->sym;

	if (n->child == NO_NODE) return;
	if (type_dyn_rank(s->type) > 0 || type_kind(s->type) == TK_SLICE
			|| type_kind(s->type) == TK_ARRAY || type_kind(s->type) == TK_STRUCT) {
		uint32_t base = new_reg(ir, TYPE_i64);

		make_addr_global(ir, base, s->ir_id, s->type);
		if (type_dyn_rank(s->type) > 0 || type_kind(s->type) == TK_SLICE)
			gen_slice_desc(ir, base, ir->ast->nodes + n->child, s->type);
		else if (type_kind(s->type) == TK_STRUCT)
			gen_struct_into(ir, base, n->child, s->type, 1);
		else gen_array_init(ir, base, n->child, s->type, 0);
		return;
	}
	uint32_t val = gen_expr(ir, ir->ast->nodes + n->child);

	make_str_global(ir, s->ir_id, val, s->type);
}

static void gen_init_fn (IR *ir)
{
	ir->reg_count = 0;
	ir->reg_types = NULL;
	ir->reg_type_cap = 0;
	ir->addressed = NULL;
	ir->addressed_cap = 0;
	ir->no_tail = 0;
	ir->sret = NO_REG;
	ir->fns[ir->init_fn].start = ir->instr_count;

	for (uint32_t i = 0; i < ir->static_count; i++)
		gen_var_init(ir, ir->statics[i]);
	for (uint32_t i = 0; i < ir->init_order_count; i++) {
		ASTNode *n = ir->ast->nodes + ir->init_order[i];

		if (ir->symtab->symbols[n->sym].is_static) continue;
		gen_var_init(ir, ir->init_order[i]);
	}
	fn_end(ir, ir->init_fn);
}

static void gen_fns (IR *ir)
{
	uint32_t idx = ir->ast->nodes[0].child;
	while (idx != NO_NODE) {
		ASTNode *n = ir->ast->nodes + idx;
		if (n->type == NODE_FN_DEC)
			gen_fn(ir, n, ir->symtab->symbols[n->sym].ir_id);
		idx = n->next_bro;
	}
}

void ir_gen (IR *ir)
{
	IRFn init_fn = {
		.name = INIT_FN_NAME,
		.len = sizeof(INIT_FN_NAME) - 1,
		.ret_type = TYPE_VOID
	};

	ir->init_fn = push_fn(ir, init_fn);
	ir_scan_structs(ir);
	declare_globals(ir);
	gen_init_fn(ir);
	gen_fns(ir);
}
