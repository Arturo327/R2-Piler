#include "ir/ir.h"
#include "ir/internal.h"

#include <assert.h>
#include <string.h>

#define LOGIC_BUDGET 8

static const uint8_t pure_node[NODE_COUNT] = {
	[NODE_LIT_CHAR] = 1, [NODE_LIT_i64] = 1, [NODE_LIT_u64] = 1,
	[NODE_ID] = 1, [NODE_CAST] = 1,
	[NODE_ADD] = 1, [NODE_SUB] = 1, [NODE_MUL] = 1,
	[NODE_AND_A] = 1, [NODE_OR_A] = 1, [NODE_XOR] = 1,
	[NODE_RS] = 1, [NODE_LS] = 1,
	[NODE_NEG] = 1, [NODE_NOT_A] = 1, [NODE_NOT_L] = 1,
	[NODE_EQ] = 1, [NODE_NE] = 1, [NODE_GT] = 1,
	[NODE_GE] = 1, [NODE_LT] = 1, [NODE_LE] = 1,
	[NODE_AND_L] = 1, [NODE_OR_L] = 1
};

static const uint8_t node_to_ir[NODE_COUNT] = {
	[NODE_ADD] = IR_ADD, [NODE_SUB] = IR_SUB, [NODE_MUL] = IR_MUL,
	[NODE_DIV] = IR_DIV, [NODE_MOD] = IR_MOD,
	[NODE_AND_A] = IR_AND_A, [NODE_OR_A] = IR_OR_A, [NODE_XOR] = IR_XOR,
	[NODE_RS] = IR_RS, [NODE_LS] = IR_LS,
	[NODE_ADD_ASSIGN] = IR_ADD, [NODE_SUB_ASSIGN] = IR_SUB,
	[NODE_MUL_ASSIGN] = IR_MUL, [NODE_DIV_ASSIGN] = IR_DIV,
	[NODE_MOD_ASSIGN] = IR_MOD, [NODE_AND_A_ASSIGN] = IR_AND_A,
	[NODE_OR_A_ASSIGN] = IR_OR_A, [NODE_XOR_ASSIGN] = IR_XOR,
	[NODE_LS_ASSIGN] = IR_LS, [NODE_RS_ASSIGN] = IR_RS,
	[NODE_NEG] = IR_NEG, [NODE_NOT_A] = IR_NOT_A, [NODE_NOT_L] = IR_NOT_L,
	[NODE_EQ] = IR_EQ, [NODE_NE] = IR_NE, [NODE_GT] = IR_GT,
	[NODE_GE] = IR_GE, [NODE_LT] = IR_LT, [NODE_LE] = IR_LE
};

static uint32_t load_var (IR *ir, Symbol *s)
{
	if (!s->depth || s->is_static)
		return make_ld_global(ir, s->ir_id, s->type);

	uint32_t tmp = new_reg(ir, s->type);
	make_move(ir, tmp, s->ir_id, s->type);
	return tmp;
}

static uint32_t gen_unary (IR *ir, ASTNode *n)
{
	ASTNode *operand = ir->ast->nodes + n->child;
	uint32_t a = gen_expr(ir, operand);

	return make_op(ir, node_to_ir[n->type], a, NO_REG, operand->data_type);
}

uint32_t ptr_scale (IR *ir, uint32_t val, TypeId ptr_type, int up)
{
	if (type_kind(ptr_type) != TK_PTR) return val;
	uint32_t size = types[types[ptr_type].base].size;
	if (size < 2) return val;
	uint8_t op = up ? IR_MUL : IR_DIV;
	return make_op(ir, op, val, make_const(ir, size, TYPE_i64), TYPE_i64);
}

static int is_ptr_arith (ASTNode *n, ASTNode *l, ASTNode *r)
{
	if (n->type != NODE_ADD && n->type != NODE_SUB) return 0;
	return type_kind(l->data_type) == TK_PTR || type_kind(r->data_type) == TK_PTR;
}

static uint32_t gen_ptr_arith (IR *ir, ASTNode *n, ASTNode *l, ASTNode *r)
{
	int ptr_left = type_kind(l->data_type) == TK_PTR;
	TypeId type = ptr_left ? l->data_type : r->data_type;
	uint32_t a = gen_expr(ir, l);
	uint32_t b = gen_expr(ir, r);

	if (ptr_left && type_kind(r->data_type) == TK_PTR)
		return ptr_scale(ir, make_op(ir, IR_SUB, a, b, TYPE_i64), type, 0);
	if (ptr_left)
		return make_op(ir, node_to_ir[n->type], a, ptr_scale(ir, b, type, 1), type);
	return make_op(ir, IR_ADD, ptr_scale(ir, a, type, 1), b, type);
}

static uint32_t gen_binary (IR *ir, ASTNode *n)
{
	ASTNode *l = ir->ast->nodes + n->child;
	ASTNode *r = ir->ast->nodes + l->next_bro;

	if (is_ptr_arith(n, l, r)) return gen_ptr_arith(ir, n, l, r);
	uint32_t a = gen_expr(ir, l);
	uint32_t b = gen_expr(ir, r);

	return make_op(ir, node_to_ir[n->type], a, b, l->data_type);
}

static void gen_jump_logic (IR *ir, ASTNode *n, uint32_t label, int when)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	ASTNode *rhs = ir->ast->nodes + lhs->next_bro;
	int stop = n->type == NODE_OR_L;
	uint32_t skip;

	if (when == stop) {
		gen_jump_if(ir, lhs, label, stop);
		gen_jump_if(ir, rhs, label, when);
		return;
	}
	skip = ir->label_count++;
	gen_jump_if(ir, lhs, skip, stop);
	gen_jump_if(ir, rhs, label, when);
	make_label(ir, skip);
}

void gen_jump_if (IR *ir, ASTNode *n, uint32_t label, int when)
{
	if (n->type == NODE_NOT_L) {
		gen_jump_if(ir, ir->ast->nodes + n->child, label, !when);
	} else if (n->type == NODE_AND_L || n->type == NODE_OR_L) {
		gen_jump_logic(ir, n, label, when);
	} else {
		uint32_t reg = gen_expr(ir, n);
		make_jump(ir, when ? IR_JNZ : IR_JZ, reg, label);
	}
}

static int is_pure (IR *ir, uint32_t idx, int *budget)
{
	ASTNode *n = ir->ast->nodes + idx;
	uint32_t c;

	if (--*budget < 0 || !pure_node[n->type]) return 0;
	for (c = n->child; c != NO_NODE; c = ir->ast->nodes[c].next_bro)
		if (!is_pure(ir, c, budget)) return 0;
	return 1;
}

static int is_simple_val (IR *ir, uint32_t idx)
{
	ASTNode *n = ir->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64: case NODE_LIT_CHAR:
		return 1;
	case NODE_ID:
		return ir->symtab->symbols[n->sym].depth != 0;
	default:
		return 0;
	}
}

static ASTNode *lone_assign (IR *ir, uint32_t idx)
{
	ASTNode *nodes = ir->ast->nodes;
	ASTNode *n = nodes + idx;

	if (n->type == NODE_BLOCK) {
		if (n->child == NO_NODE || nodes[n->child].next_bro != NO_NODE) return NULL;
		n = nodes + n->child;
	}
	if (n->type != NODE_ASSIGN) return NULL;

	Symbol *s = ir->symtab->symbols + nodes[n->child].sym;

	if (!s->depth || s->is_static) return NULL;
	return is_simple_val(ir, nodes[n->child].next_bro) ? n : NULL;
}

static int select_arms (IR *ir, ASTNode *n, ASTNode **a, ASTNode **b)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t body = nodes[n->child].next_bro;
	uint32_t other = nodes[body].next_bro;

	*b = NULL;
	*a = lone_assign(ir, body);
	if (!*a) return 0;
	if (other == NO_NODE) return 1;
	if (nodes[other].type != NODE_ELSE || nodes[other].next_bro != NO_NODE) return 0;
	*b = lone_assign(ir, nodes[other].child);
	return *b && nodes[(*b)->child].sym == nodes[(*a)->child].sym;
}

int gen_select (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	ASTNode *a;
	ASTNode *b;
	int budget = LOGIC_BUDGET;

	if (!select_arms(ir, n, &a, &b)) return 0;
	Symbol *s = ir->symtab->symbols + nodes[a->child].sym;
	int late = is_pure(ir, n->child, &budget);

	uint32_t vc = NO_REG;
	if (!late) vc = gen_expr(ir, nodes + n->child);
	uint32_t va = gen_expr(ir, nodes + nodes[a->child].next_bro);
	uint32_t vb = b ? gen_expr(ir, nodes + nodes[b->child].next_bro) : s->ir_id;
	if (late) vc = gen_expr(ir, nodes + n->child);

	make_select(ir, s->ir_id, va, vb, vc, s->type);
	return 1;
}

static uint32_t gen_ternary (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t then_idx = nodes[n->child].next_bro;
	uint32_t else_idx = nodes[then_idx].next_bro;

	if (ir->use_select) {
		int cbudget = LOGIC_BUDGET;
		int abudget = LOGIC_BUDGET;
		int bbudget = LOGIC_BUDGET;

		if (is_pure(ir, n->child, &cbudget) && is_pure(ir, then_idx, &abudget)
				&& is_pure(ir, else_idx, &bbudget)) {
			uint32_t va = gen_expr(ir, nodes + then_idx);
			uint32_t vb = gen_expr(ir, nodes + else_idx);
			uint32_t vc = gen_expr(ir, nodes + n->child);

			uint32_t res = new_reg(ir, n->data_type);
			make_select(ir, res, va, vb, vc, n->data_type);
			return res;
		}
	}

	uint32_t l_else = ir->label_count++;
	uint32_t l_end = ir->label_count++;

	uint32_t res = new_reg(ir, n->data_type);
	gen_jump_if(ir, nodes + n->child, l_else, 0);
	make_move(ir, res, gen_expr(ir, nodes + then_idx), n->data_type);
	make_jump(ir, IR_JMP, NO_REG, l_end);
	make_label(ir, l_else);
	make_move(ir, res, gen_expr(ir, nodes + else_idx), n->data_type);
	make_label(ir, l_end);
	return res;
}

static uint32_t gen_bool (IR *ir, ASTNode *n)
{
	uint32_t val = gen_expr(ir, n);
	uint32_t zero;

	if (n->type >= NODE_EQ && n->type <= NODE_NOT_L) return val;
	zero = make_const(ir, 0, n->data_type);
	return make_op(ir, IR_NE, val, zero, n->data_type);
}

static uint32_t gen_logic_flat (IR *ir, ASTNode *n)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	ASTNode *rhs = ir->ast->nodes + lhs->next_bro;
	uint32_t a = gen_bool(ir, lhs);
	uint32_t b = gen_bool(ir, rhs);
	uint8_t op = n->type == NODE_AND_L ? IR_AND_A : IR_OR_A;

	return make_op(ir, op, a, b, TYPE_i64);
}

static uint32_t gen_logic (IR *ir, ASTNode *n)
{
	int budget = LOGIC_BUDGET;
	uint32_t rhs = ir->ast->nodes[n->child].next_bro;
	uint32_t res, l_false, l_end;

	if (is_pure(ir, rhs, &budget)) return gen_logic_flat(ir, n);

	res = new_reg(ir, TYPE_i64);
	l_false = ir->label_count++;
	l_end = ir->label_count++;
	gen_jump_if(ir, n, l_false, 0);
	make_const_into(ir, res, 1, TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, l_end);
	make_label(ir, l_false);
	make_const_into(ir, res, 0, TYPE_i64);
	make_label(ir, l_end);
	return res;
}

static uint32_t gen_cast (IR *ir, ASTNode *n)
{
	ASTNode *operand = ir->ast->nodes + n->child;
	uint32_t src = gen_expr(ir, operand);

	if (type_kind(operand->data_type) == TK_ARRAY
			|| type_kind(operand->data_type) == TK_SLICE
			|| type_kind(operand->data_type) == TK_STRUCT)
		return src;
	if (types[n->data_type].size == 8 && types[operand->data_type].size == 8) {
		ir->reg_types[src] = n->data_type;
		return src;
	}
	if (n->data_type == operand->data_type) return src;

	return make_op(ir, IR_EXTEND, src, NO_REG, n->data_type);
}

static uint32_t gen_compound_assign (IR *ir, ASTNode *n)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	ASTNode *rhs = ir->ast->nodes + lhs->next_bro;

	if (lhs->flags & NODE_FLAG_BITFIELD) {
		const StructField *f = find_field(ir->ast->nodes[lhs->child].data_type, lhs->str, lhs->len);
		uint32_t addr = gen_field_addr(ir, lhs);
		uint32_t a = bitfield_extract(ir, make_ld_ptr(ir, addr, f->type), f);
		uint32_t b = gen_expr(ir, rhs);

		return gen_bitfield_store(ir, addr, f, make_op(ir, node_to_ir[n->type], a, b, f->type));
	}

	uint32_t addr = gen_lvalue_addr(ir, lhs);
	uint32_t a = addr != NO_REG ? make_ld_ptr(ir, addr, lhs->data_type)
			: load_var(ir, ir->symtab->symbols + lhs->sym);
	uint32_t b = ptr_scale(ir, gen_expr(ir, rhs), lhs->data_type, 1);
	uint32_t val = make_op(ir, node_to_ir[n->type], a, b, lhs->data_type);

	if (addr != NO_REG) {
		make_str_ptr(ir, addr, val, lhs->data_type);
		return val;
	}

	Symbol *s = ir->symtab->symbols + lhs->sym;
	if (!s->depth || s->is_static) make_str_global(ir, s->ir_id, val, s->type);
	else make_move(ir, s->ir_id, val, s->type);
	return val;
}

void gen_inc_dec (IR *ir, ASTNode *n)
{
	ASTNode *target = ir->ast->nodes + n->child;
	TypeId type = target->data_type;
	uint32_t addr = gen_lvalue_addr(ir, target);

	if (target->flags & NODE_FLAG_BITFIELD) {
		const StructField *f = find_field(ir->ast->nodes[target->child].data_type, target->str, target->len);
		uint32_t a = bitfield_extract(ir, make_ld_ptr(ir, addr, f->type), f);
		uint32_t one = make_const(ir, 1, f->type);

		gen_bitfield_store(ir, addr, f, make_op(ir, n->type == NODE_INC ? IR_ADD : IR_SUB, a, one, f->type));
		return;
	}
	uint32_t step = 1;
	uint32_t a = addr != NO_REG ? make_ld_ptr(ir, addr, type)
			: load_var(ir, ir->symtab->symbols + target->sym);

	if (type_kind(type) == TK_PTR)
		step = types[types[type].base].size;

	uint32_t val = make_op(ir, n->type == NODE_INC ? IR_ADD : IR_SUB, a,
			make_const(ir, step, type), type);
	if (addr != NO_REG) {
		make_str_ptr(ir, addr, val, type);
		return;
	}

	Symbol *s = ir->symtab->symbols + target->sym;
	if (!s->depth || s->is_static) make_str_global(ir, s->ir_id, val, s->type);
	else make_move(ir, s->ir_id, val, s->type);
}

uint32_t gen_expr (IR *ir, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_LIT_i64: return make_const(ir, n->i64, n->data_type);
	case NODE_LIT_u64: return make_const(ir, (int64_t)n->u64, n->data_type);
	case NODE_LIT_CHAR: return make_const(ir, (int8_t)n->chr, n->data_type);

	case NODE_ID: {
		Symbol *s = ir->symtab->symbols + n->sym;

		if (type_dyn_rank(s->type) > 0) {
			if (s->kind == SYMBOL_PARAM)
				return read_reg(ir, s->ir_id,
						type_ptr_to(type_flat_element(s->type)));
			return make_ld_ptr(ir, local_storage_addr(ir, s, s->type),
					type_ptr_to(type_flat_element(s->type)));
		}
		if (type_kind(s->type) == TK_ARRAY || type_kind(s->type) == TK_STRUCT)
			return gen_array_addr(ir, n);
		if (type_kind(s->type) == TK_SLICE)
			return read_reg(ir, s->ir_id,
					type_ptr_to(type_flat_element(s->type)));
		return load_var(ir, s);
	}
	case NODE_DOT: {
		TypeId type = n->data_type;

		if (type_kind(ir->ast->nodes[n->child].data_type) == TK_STRUCT)
			return gen_field(ir, n);

		if (type_kind(type) == TK_UNTYPED_INT) type = TYPE_i64;
		return slice_dim_reg(ir, ir->ast->nodes + n->child, 0, type);
	}
	case NODE_DEREF: return gen_deref(ir, n);
	case NODE_INDEX:
		if (type_is_aggregate(n->data_type))
			return gen_index_addr(ir, n);
		return make_ld_ptr(ir, gen_index_addr(ir, n), n->data_type);
	case NODE_ADDR: return gen_addr(ir, n);
	case NODE_LIT_NULL: return make_const(ir, 0, TYPE_NULL);
	case NODE_LIT_STR: return make_str_lit(ir, n);
	case NODE_FN_CALL: return gen_call_value(ir, n);
	case NODE_ASSIGN: return gen_assign(ir, n);
	case NODE_ADD_ASSIGN: case NODE_SUB_ASSIGN: case NODE_MUL_ASSIGN:
	case NODE_DIV_ASSIGN: case NODE_MOD_ASSIGN: case NODE_AND_A_ASSIGN:
	case NODE_OR_A_ASSIGN: case NODE_XOR_ASSIGN: case NODE_LS_ASSIGN:
	case NODE_RS_ASSIGN: return gen_compound_assign(ir, n);

	case NODE_NEG: case NODE_NOT_A: case NODE_NOT_L: return gen_unary(ir, n);
	case NODE_AND_L: case NODE_OR_L: return gen_logic(ir, n);
	case NODE_CAST: return gen_cast(ir, n);
	case NODE_TERNARY: return gen_ternary(ir, n);
	case NODE_STRUCT_INIT: {
		uint32_t tmp = new_struct_temp(ir, n->data_type);
		gen_struct_init(ir, tmp, n, n->data_type);
		return tmp;
	}

	default:
		assert(node_to_ir[n->type] != IR_NOP);
		return gen_binary(ir, n);
	}
}
