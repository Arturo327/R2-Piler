#include "ir/ir.h"
#include "ir/internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void gen_stmt (IR *ir, ASTNode *n);
static void gen_block (IR *ir, ASTNode *parent);

void gen_var_dec (IR *ir, ASTNode *n)
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
		if (n->sym == ir->nrvo) {
			s->ir_id = ir->sret;
			if (n->child != NO_NODE)
				gen_struct_into(ir, ir->sret, n->child, s->type, 1);
			return;
		}
		reg = new_reg(ir, s->type);
		s->ir_id = reg;
		ir->addressed[reg] = 1;
		ir->no_tail = 1;
		if (n->child != NO_NODE) {
			ir->scalarize = !s->escaped;
			gen_struct_into(ir, local_storage_addr(ir, s, s->type), n->child, s->type, 1);
			ir->scalarize = 0;
		}
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

static void gen_ret_slot (IR *ir, uint32_t dst, uint32_t val_idx, TypeId type)
{
	ASTNode *val = ir->ast->nodes + val_idx;

	if (type_dyn_rank(type) > 0) {
		gen_slice_desc(ir, dst, val, type);
		return;
	}
	if (type_kind(type) == TK_STRUCT) {
		gen_struct_into(ir, dst, val_idx, type, 1);
		return;
	}
	make_str_ptr(ir, dst, gen_expr(ir, val), ret_slot_type(type));
}

static void gen_multi_return (IR *ir, ASTNode *n, uint32_t ret, uint32_t count)
{
	uint32_t val = n->child;
	uint32_t i = 0;

	while (val != NO_NODE && i < count) {
		uint32_t at = add_offset(ir, ir->sret, ret_off(ir, ret, i));

		gen_ret_slot(ir, at, val, ir->ast->nodes[ret_node_at(ir, ret, i)].data_type);
		val = ir->ast->nodes[val].next_bro;
		i++;
	}
	make_ret(ir, NO_REG, TYPE_VOID);
}

static void gen_ret_regs (IR *ir, uint32_t val, TypeId type)
{
	uint32_t words[2];
	uint32_t src = gen_expr(ir, ir->ast->nodes + val);

	pack_struct(ir, src, type, words);
	make_ret2(ir, words[0], words[1], TYPE_u64);
}

static void gen_single_return (IR *ir, ASTNode *n, uint32_t ret)
{
	TypeId type = ir->ast->nodes[ret].data_type;
	ASTNode *val = ir->ast->nodes + n->child;

	if (ir->sret != NO_REG) {
		if (val->type == NODE_ID && val->sym == ir->nrvo) {
			make_ret(ir, NO_REG, TYPE_VOID);
			return;
		}
		if (type_dyn_rank(type) > 0)
			gen_slice_desc(ir, ir->sret, ir->ast->nodes + n->child, type);
		else gen_struct_into(ir, ir->sret, n->child, type, 1);
		make_ret(ir, NO_REG, TYPE_VOID);
		return;
	}
	if (struct_eightbytes(type)) {
		gen_ret_regs(ir, n->child, type);
		return;
	}
	make_ret(ir, gen_expr(ir, ir->ast->nodes + n->child), ret_slot_type(type));
}

static void gen_return (IR *ir, ASTNode *n)
{
	uint32_t ret = ir->ret_node;
	uint32_t count = ret == NO_NODE ? 1 : ret_count(ir, ret);

	if (n->child == NO_NODE) {
		make_ret(ir, NO_REG, TYPE_VOID);
		return;
	}
	if (count > 1) {
		gen_multi_return(ir, n, ret, count);
		return;
	}
	gen_single_return(ir, n, ret);
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

static uint32_t push_loop (IR *ir, uint32_t break_label, uint32_t cont_label)
{
	if (ir->loop_count == ir->loop_cap) {
		uint32_t old_cap = ir->loop_cap;
		ir->loop_cap = old_cap ? old_cap << 1 : 8;
		ir->loops = arena_realloc(ir->arena, ir->loops,
				old_cap * sizeof(IRLoop), ir->loop_cap * sizeof(IRLoop));
	}

	uint32_t slot = ir->loop_count++;

	ir->loops[slot].break_label = break_label;
	ir->loops[slot].cont_label = cont_label;
	return slot;
}

static uint32_t gen_loop_target (IR *ir, uint32_t level, int cont)
{
	uint32_t slot = ir->loop_count - level;
	uint32_t *label = cont ? &ir->loops[slot].cont_label
			: &ir->loops[slot].break_label;

	if (*label == NO_LABEL) *label = ir->label_count++;
	return *label;
}

static void gen_loop_jump (IR *ir, ASTNode *n)
{
	int cont = n->type == NODE_CONTINUE;
	uint32_t target = gen_loop_target(ir, (uint32_t)n->u64, cont);

	make_jump(ir, IR_JMP, NO_REG, target);
}

static void gen_while (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t body = nodes[n->child].next_bro;
	uint32_t els = nodes[body].next_bro;
	uint32_t l_body = ir->label_count++;
	uint32_t l_cond = ir->label_count++;
	uint32_t slot = push_loop(ir, NO_LABEL, l_cond);

	make_jump(ir, IR_JMP, NO_REG, l_cond);
	make_label(ir, l_body);
	gen_stmt(ir, nodes + body);
	make_label(ir, l_cond);
	gen_jump_if(ir, nodes + n->child, l_body, 1);

	uint32_t brk = ir->loops[slot].break_label;

	ir->loop_count--;
	if (els != NO_NODE) gen_stmt(ir, nodes + nodes[els].child);
	if (brk != NO_LABEL) make_label(ir, brk);
}

static void gen_for (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	ASTNode *cond = nodes + nodes[n->child].next_bro;
	ASTNode *updt = nodes + cond->next_bro;
	ASTNode *body = nodes + updt->next_bro;
	uint32_t els = body->next_bro;
	uint32_t l_body = ir->label_count++;
	uint32_t l_cond = ir->label_count++;
	uint32_t slot = push_loop(ir, NO_LABEL, NO_LABEL);

	gen_stmt(ir, nodes + n->child);
	if (cond->type != NODE_EMPTY)
		make_jump(ir, IR_JMP, NO_REG, l_cond);

	make_label(ir, l_body);
	gen_stmt(ir, body);
	if (ir->loops[slot].cont_label != NO_LABEL)
		make_label(ir, ir->loops[slot].cont_label);
	gen_stmt(ir, updt);
	make_label(ir, l_cond);
	if (cond->type == NODE_EMPTY) make_jump(ir, IR_JMP, NO_REG, l_body);
	else gen_jump_if(ir, cond, l_body, 1);

	uint32_t brk = ir->loops[slot].break_label;

	ir->loop_count--;
	if (els != NO_NODE) gen_stmt(ir, nodes + nodes[els].child);
	if (brk != NO_LABEL) make_label(ir, brk);
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

static void gen_stmt_inner (IR *ir, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_VAR_DEC: gen_var_dec(ir, n); return;
	case NODE_ARR_DEC: gen_var_dec(ir, n); return;
	case NODE_BLOCK: gen_block(ir, n); return;
	case NODE_RET: gen_return(ir, n); return;
	case NODE_WHILE: gen_while(ir, n); return;
	case NODE_FOR: gen_for(ir, n); return;
	case NODE_BREAK: case NODE_CONTINUE: gen_loop_jump(ir, n); return;
	case NODE_IF: gen_if(ir, n); return;
	case NODE_SWITCH: gen_switch(ir, n); return;
	case NODE_INC: case NODE_DEC: gen_inc_dec(ir, n); return;
	case NODE_MULTI_ASSIGN: gen_multi_assign(ir, n); return;
	case NODE_EMPTY: return;
	default: gen_expr(ir, n); return;
	}
}

static void gen_stmt (IR *ir, ASTNode *n)
{
	gen_stmt_inner(ir, n);
	for (uint32_t i = 0; i < ir->temp_count; i++)
		ir->temps[i].busy = 0;
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

static uint32_t dec_struct_regs (IR *ir, Symbol *s, uint32_t idx)
{
	uint32_t n8 = struct_eightbytes(s->type);

	s->ir_id = new_reg(ir, TYPE_u64);
	make_arg_dec(ir, s->ir_id, idx++, TYPE_u64);
	if (n8 == 2) make_arg_dec(ir, new_reg(ir, TYPE_u64), idx++, TYPE_u64);
	return idx;
}

static void spill_struct_params (IR *ir, uint32_t node)
{
	uint32_t p = ir->ast->nodes[node].child;

	for (; p != NO_NODE; p = ir->ast->nodes[p].next_bro) {
		Symbol *s = ir->symtab->symbols + ir->ast->nodes[p].sym;
		uint32_t words[2] = { s->ir_id, s->ir_id + 1 };
		uint32_t obj;

		if (!struct_eightbytes(s->type)) continue;
		if (struct_eightbytes(s->type) == 1) words[1] = NO_REG;
		s->ir_id = new_reg(ir, s->type);
		obj = local_storage_addr(ir, s, s->type);
		unpack_struct(ir, obj, s->type, words);
		s->ir_id = obj;
	}
}

static void gen_args_dec (IR *ir, uint32_t node)
{
	uint32_t p = ir->ast->nodes[node].child;
	uint32_t idx = ir->sret == NO_REG ? 0 : 1;

	while (p != NO_NODE) {
		ASTNode *n = ir->ast->nodes + p;
		Symbol *s = ir->symtab->symbols + n->sym;
		TypeKind kind = type_kind(s->type);

		if (struct_eightbytes(s->type)) {
			idx = dec_struct_regs(ir, s, idx);
		} else if (kind == TK_ARRAY || kind == TK_SLICE || kind == TK_STRUCT) {
			idx = dec_aggregate_param(ir, s, idx);
		} else {
			s->ir_id = new_reg(ir, s->type);
			if (s->addressed) ir->addressed[s->ir_id] = 1;
			make_arg_dec(ir, s->ir_id, idx++, s->type);
		}
		p = n->next_bro;
	}
	spill_struct_params(ir, node);
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

static int nrvo_scan (IR *ir, uint32_t idx, uint32_t *sym)
{
	ASTNode *n = ir->ast->nodes + idx;

	if (n->type == NODE_RET) {
		ASTNode *val = n->child == NO_NODE ? NULL : ir->ast->nodes + n->child;
		if (!val || val->type != NODE_ID || val->next_bro != NO_NODE) return 0;
		if (*sym != NO_SYMBOL && *sym != val->sym) return 0;
		*sym = val->sym;
		return 1;
	}
	for (uint32_t k = n->child; k != NO_NODE; k = ir->ast->nodes[k].next_bro) {
		if (!nrvo_scan(ir, k, sym)) return 0;
	}
	return 1;
}

static uint32_t find_nrvo (IR *ir, uint32_t ret, uint32_t body)
{
	TypeId type = ir->ast->nodes[ret].data_type;

	if (ir->sret == NO_REG || type_kind(type) != TK_STRUCT) return NO_SYMBOL;
	if (ir->ast->nodes[ret].child != NO_NODE || struct_is_small(type)) return NO_SYMBOL;

	uint32_t sym = NO_SYMBOL;
	if (!nrvo_scan(ir, body, &sym) || sym == NO_SYMBOL) return NO_SYMBOL;

	Symbol *s = ir->symtab->symbols + sym;
	if (s->kind != SYMBOL_VAR || !s->depth || s->is_static) return NO_SYMBOL;
	return s->type == type ? sym : NO_SYMBOL;
}

static void gen_fn (IR *ir, ASTNode *n, uint32_t fn_idx)
{
	uint32_t args = n->child;
	uint32_t ret = ir->ast->nodes[args].next_bro;
	uint32_t body = ir->ast->nodes[ret].next_bro;

	ir->reg_count = 0;
	ir->reg_types = NULL;
	ir->reg_type_cap = 0;
	ir->addressed = NULL;
	ir->addressed_cap = 0;
	ir->no_tail = 0;
	ir->temp_count = 0;
	ir->loop_count = 0;
	ir->sret = NO_REG;
	ir->ret_node = ret;
	memset(ir->desc, 0xFF, sizeof(uint32_t) * ir->ast->count);

	ir->fns[fn_idx].start = ir->instr_count;
	if (fn_uses_sret(ir, ret)) {
		TypeId ptr = type_ptr_to(ret_buf_type(ir, ret));

		ir->sret = new_reg(ir, ptr);
		make_arg_dec(ir, ir->sret, 0, ptr);
	}
	gen_args_dec(ir, args);
	ir->nrvo = find_nrvo(ir, ret, body);
	gen_block(ir, ir->ast->nodes + body);
	fn_end(ir, fn_idx);
}

static int bind_image (IR *ir, ASTNode *n, Symbol *s)
{
	IRGlobal *g = ir->globals + s->ir_id;
	TypeKind kind = type_kind(s->type);
	uint8_t *img;

	if (kind != TK_STRUCT && kind != TK_ARRAY) return 0;
	if (type_dyn_rank(s->type) > 0) return 0;
	img = const_image(ir, n->child, s->type);
	if (!img) return 0;
	g->image = img;
	g->init_kind = IR_INIT_IMAGE;
	g->has_init = 1;
	return 1;
}

static void gen_var_init (IR *ir, uint32_t idx)
{
	ASTNode *n = ir->ast->nodes + idx;
	Symbol *s = ir->symtab->symbols + n->sym;

	if (n->child == NO_NODE) return;
	if (bind_image(ir, n, s)) return;
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
	ir->temp_count = 0;
	ir->sret = NO_REG;
	ir->ret_node = NO_NODE;
	memset(ir->desc, 0xFF, sizeof(uint32_t) * ir->ast->count);
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
