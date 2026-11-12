#include "ir/ir.h"
#include "ir/internal.h"

static int mentions_sym (IR *ir, uint32_t idx, uint32_t sym)
{
	ASTNode *n = ir->ast->nodes + idx;

	if (n->type == NODE_ID && n->sym == sym) return 1;
	for (uint32_t k = n->child; k != NO_NODE; k = ir->ast->nodes[k].next_bro) {
		if (mentions_sym(ir, k, sym)) return 1;
	}
	return 0;
}

static int dest_is_private (IR *ir, ASTNode *lhs)
{
	uint32_t root = agg_root_sym(ir->ast, (uint32_t)(lhs - ir->ast->nodes));
	uint32_t call = lhs->next_bro;

	if (root == NO_SYMBOL || ir->ast->nodes[call].type != NODE_FN_CALL) return 0;
	Symbol *s = ir->symtab->symbols + root;
	if (s->kind != SYMBOL_VAR || !s->depth || s->is_static || s->escaped) return 0;
	if (struct_is_small(lhs->data_type)) return 0;
	return !mentions_sym(ir, call, root);
}

uint32_t gen_assign (IR *ir, ASTNode *n)
{
	ASTNode *lhs = ir->ast->nodes + n->child;

	if ((lhs->type == NODE_DOT || lhs->type == NODE_ID) && type_dyn_rank(lhs->data_type) > 0) {
		uint32_t dst;
		if (lhs->type == NODE_DOT) dst = gen_lvalue_addr(ir, lhs);
		else {
			Symbol *s = ir->symtab->symbols + lhs->sym;
			if (!s->depth || s->is_static) {
				dst = new_reg(ir, TYPE_i64);
				make_addr_global(ir, dst, s->ir_id, s->type);
			} else dst = local_storage_addr(ir, s, s->type);
		}
		ASTNode *rhs = ir->ast->nodes + lhs->next_bro;
		return gen_slice_desc(ir, dst, rhs, lhs->data_type);
	}

	if (lhs->flags & NODE_FLAG_BITFIELD) {
		const StructField *f = find_field(ir->ast->nodes[lhs->child].data_type, lhs->str, lhs->len);
		uint32_t addr = gen_field_addr(ir, lhs);
		uint32_t val = gen_expr(ir, ir->ast->nodes + lhs->next_bro);

		return gen_bitfield_store(ir, addr, f, val);
	}

	if (type_kind(lhs->data_type) == TK_STRUCT) {
		uint32_t dst = gen_expr(ir, lhs);
		gen_struct_into(ir, dst, lhs->next_bro, lhs->data_type, dest_is_private(ir, lhs));
		return dst;
	}

	uint32_t addr = gen_lvalue_addr(ir, lhs);
	uint32_t val = gen_expr(ir, ir->ast->nodes + lhs->next_bro);

	if (addr != NO_REG) {
		make_str_ptr(ir, addr, val, lhs->data_type);
		return val;
	}

	Symbol *s = ir->symtab->symbols + lhs->sym;
	if (!s->depth || s->is_static) make_str_global(ir, s->ir_id, val, s->type);
	else make_move(ir, s->ir_id, val, s->type);
	return val;
}

static uint32_t gen_target_addr (IR *ir, ASTNode *t)
{
	uint32_t addr = gen_lvalue_addr(ir, t);

	if (addr != NO_REG) return addr;

	Symbol *s = ir->symtab->symbols + t->sym;
	if (!s->depth || s->is_static) {
		uint32_t dst = new_reg(ir, TYPE_i64);

		make_addr_global(ir, dst, s->ir_id, s->type);
		return dst;
	}
	return local_storage_addr(ir, s, s->type);
}

static uint32_t convert_value (IR *ir, uint32_t val, TypeId from, TypeId to)
{
	if (from == to) return val;
	if (types[from].size == 8 && types[to].size == 8) {
		ir->reg_types[val] = to;
		return val;
	}
	return make_op(ir, IR_EXTEND, val, NO_REG, to);
}

static void gen_multi_store (IR *ir, uint32_t target, uint32_t at, TypeId type)
{
	ASTNode *t = ir->ast->nodes + target;
	TypeId dst = t->data_type;

	if (t->type == NODE_VAR_DEC || t->type == NODE_ARR_DEC) gen_var_dec(ir, t);
	if (type_is_aggregate(dst)) {
		gen_copy(ir, gen_target_addr(ir, t), at, dst);
		return;
	}

	uint32_t val = convert_value(ir, make_ld_ptr(ir, at, type), type, dst);
	uint32_t addr = gen_lvalue_addr(ir, t);

	if (addr != NO_REG) {
		make_str_ptr(ir, addr, val, dst);
		return;
	}
	Symbol *s = ir->symtab->symbols + t->sym;
	if (!s->depth || s->is_static) make_str_global(ir, s->ir_id, val, dst);
	else make_move(ir, s->ir_id, val, dst);
}

void gen_multi_assign (IR *ir, ASTNode *n)
{
	uint32_t rhs = n->child;

	while (ir->ast->nodes[rhs].next_bro != NO_NODE)
		rhs = ir->ast->nodes[rhs].next_bro;

	uint32_t ret = call_ret_node(ir, ir->ast->nodes + rhs);
	uint32_t count = ret == NO_NODE ? 0 : ret_count(ir, ret);
	uint32_t buf = gen_call(ir, ir->ast->nodes + rhs, NO_REG);
	uint32_t target = n->child;
	uint32_t i = 0;

	while (target != rhs) {
		uint32_t next = ir->ast->nodes[target].next_bro;

		if (i < count)
			gen_multi_store(ir, target, add_offset(ir, buf, ret_off(ir, ret, i)),
					ir->ast->nodes[ret_node_at(ir, ret, i)].data_type);
		target = next;
		i++;
	}
}
