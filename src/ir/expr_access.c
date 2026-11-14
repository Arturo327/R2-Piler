#include "ir/ir.h"
#include "ir/internal.h"

uint32_t read_reg (IR *ir, uint32_t reg, TypeId type)
{
	uint32_t tmp = new_reg(ir, type);

	make_move(ir, tmp, reg, type);
	return tmp;
}

uint32_t add_offset (IR *ir, uint32_t base, uint64_t off)
{
	if (!off) return base;
	return make_op(ir, IR_ADD, base, make_const(ir, (int64_t)off, TYPE_i64), TYPE_i64);
}

uint32_t local_storage_addr (IR *ir, Symbol *s, TypeId type)
{
	if ((uint32_t)(s - ir->symtab->symbols) == ir->nrvo) return ir->sret;
	uint32_t dst = new_reg(ir, TYPE_i64);

	if (!s->depth || s->is_static) {
		make_addr_global(ir, dst, s->ir_id, type);
		return dst;
	}
	ir->addressed[s->ir_id] = 1;
	make_addr_local(ir, dst, s->ir_id, type);
	return dst;
}

static uint32_t elem_scale (IR *ir, uint32_t val, TypeId elem)
{
	uint32_t size = types[elem].size;

	if (size < 2) return val;
	return make_op(ir, IR_MUL, val, make_const(ir, size, TYPE_i64), TYPE_i64);
}

uint32_t gen_field_addr (IR *ir, ASTNode *n)
{
	ASTNode *left = ir->ast->nodes + n->child;
	const StructField *f = find_field(left->data_type, n->str, n->len);
	uint32_t base = gen_expr(ir, left);

	return add_offset(ir, base, f->offset);
}

uint32_t gen_field (IR *ir, ASTNode *n)
{
	TypeId st = ir->ast->nodes[n->child].data_type;
	uint32_t addr = gen_field_addr(ir, n);

	if (n->flags & NODE_FLAG_BITFIELD) {
		const StructField *f = find_field(st, n->str, n->len);

		return bitfield_extract(ir, make_ld_ptr(ir, addr, f->type), f);
	}
	if (type_kind(n->data_type) == TK_STRUCT)
		return addr;
	if (type_dyn_rank(n->data_type) > 0)
		return make_ld_ptr(ir, addr,
				type_ptr_to(type_flat_element(n->data_type)));
	if (type_kind(n->data_type) == TK_ARRAY)
		return addr;
	return make_ld_ptr(ir, addr, n->data_type);
}

uint32_t slice_dim_reg (IR *ir, ASTNode *n, int dim, TypeId type)
{
	if (n->type == NODE_CAST && n->child != NO_NODE)
		n = ir->ast->nodes + n->child;
	ASTNode *root = n;
	TypeId t = n->data_type;

	while (root->type == NODE_INDEX) root = ir->ast->nodes + root->child;
	for (int d = 0; d < dim; d++) t = types[t].base;
	if (type_kind(t) == TK_ARRAY) return make_const(ir, types[t].count, type);

	if (root->type == NODE_ID) {
		Symbol *s = ir->symtab->symbols + root->sym;
		if (s->ir_dims)
			return read_reg(ir, s->ir_dims[type_dyn_rank(s->type) - type_dyn_rank(t)], type);
	}

	uint32_t addr;
	switch (n->type)
	{
	case NODE_ID: {
		Symbol *s = ir->symtab->symbols + n->sym;
		addr = local_storage_addr(ir, s, s->type);
		break;
	}
	case NODE_INDEX: addr = gen_index_addr(ir, n); break;
	case NODE_DOT: addr = gen_field_addr(ir, n); break;
	case NODE_DEREF: addr = gen_expr(ir, n); break;
	case NODE_FN_CALL: addr = gen_call_desc(ir, n); break;
	default: addr = gen_expr(ir, n); break;
	}

	int dyn_idx = type_dyn_rank(n->data_type) - type_dyn_rank(t);
	uint32_t off = (uint32_t)(8 * (dyn_idx + 1));
	return make_ld_ptr(ir, add_offset(ir, addr, off), type);
}

static uint32_t slice_stride (IR *ir, ASTNode *base, TypeId type)
{
	TypeId leaf = type_flat_element(type);
	int rank = type_slice_rank(type);
	uint32_t stride = slice_dim_reg(ir, base, rank - 1, TYPE_i64);

	if (types[leaf].size > 1)
		stride = make_op(ir, IR_MUL, stride,
				make_const(ir, types[leaf].size, TYPE_i64), TYPE_i64);
	for (int d = rank - 2; d >= 1; d--)
		stride = make_op(ir, IR_MUL, slice_dim_reg(ir, base, d, TYPE_i64), stride, TYPE_i64);
	return stride;
}

static uint32_t mixed_stride (IR *ir, ASTNode *base, TypeId type)
{
	uint64_t fixed = types[type_flat_element(type)].size;
	uint32_t dyn = NO_REG;
	TypeId t = types[type].base;

	for (int d = 1; type_kind(t) == TK_ARRAY || type_kind(t) == TK_SLICE; d++) {
		if (type_kind(t) == TK_ARRAY) fixed *= types[t].count;
		else if (dyn == NO_REG) dyn = slice_dim_reg(ir, base, d, TYPE_i64);
		else dyn = make_op(ir, IR_MUL, dyn,
				slice_dim_reg(ir, base, d, TYPE_i64), TYPE_i64);
		t = types[t].base;
	}
	if (dyn == NO_REG) return make_const(ir, (int64_t)fixed, TYPE_i64);
	if (fixed == 1) return dyn;
	return make_op(ir, IR_MUL, dyn, make_const(ir, (int64_t)fixed, TYPE_i64), TYPE_i64);
}

uint32_t gen_index_addr (IR *ir, ASTNode *n)
{
	ASTNode *base = ir->ast->nodes + n->child;
	TypeId type = base->data_type;
	TypeKind kind = type_kind(type);
	TypeId elem = types[type].base;
	uint32_t a = kind == TK_ARRAY ? gen_array_addr(ir, base) : gen_expr(ir, base);
	uint32_t b = gen_expr(ir, ir->ast->nodes + base->next_bro);

	if (kind == TK_PTR)
		return make_op(ir, IR_ADD, a, ptr_scale(ir, b, type, 1), type);
	if (type_is_mixed(type)) {
		uint32_t stride = mixed_stride(ir, base, type);

		return make_op(ir, IR_ADD, a,
				make_op(ir, IR_MUL, b, stride, TYPE_i64), TYPE_i64);
	}
	if (kind == TK_SLICE && type_kind(elem) == TK_SLICE) {
		uint32_t stride = slice_stride(ir, base, type);

		return make_op(ir, IR_ADD, a,
				make_op(ir, IR_MUL, b, stride, TYPE_i64), TYPE_i64);
	}
	return make_op(ir, IR_ADD, a, elem_scale(ir, b, elem), TYPE_i64);
}

uint32_t gen_array_addr (IR *ir, ASTNode *n)
{
	if (n->type != NODE_ID) return gen_expr(ir, n);

	Symbol *s = ir->symtab->symbols + n->sym;
	if (s->kind == SYMBOL_PARAM)
		return read_reg(ir, s->ir_id, type_ptr_to(type_flat_element(s->type)));
	if (!s->depth || s->is_static) {
		uint32_t dst = new_reg(ir, TYPE_i64);

		make_addr_global(ir, dst, s->ir_id, n->data_type);
		return dst;
	}
	return local_storage_addr(ir, s, n->data_type);
}

uint32_t gen_addr (IR *ir, ASTNode *n)
{
	ASTNode *child = ir->ast->nodes + n->child;

	if (child->type == NODE_DEREF)
		return gen_expr(ir, ir->ast->nodes + child->child);
	if (child->type == NODE_INDEX)
		return gen_index_addr(ir, child);
	if (child->type == NODE_DOT)
		return gen_field_addr(ir, child);

	Symbol *s = ir->symtab->symbols + child->sym;
	if (type_kind(s->type) == TK_STRUCT)
		return gen_array_addr(ir, child);
	uint32_t dst = new_reg(ir, n->data_type);
	if (!s->depth || s->is_static) {
		make_addr_global(ir, dst, s->ir_id, n->data_type);
		return dst;
	}
	ir->addressed[s->ir_id] = 1;
	make_addr_local(ir, dst, s->ir_id, n->data_type);
	return dst;
}

uint32_t gen_deref (IR *ir, ASTNode *n)
{
	uint32_t ptr = gen_expr(ir, ir->ast->nodes + n->child);

	if (type_kind(n->data_type) == TK_STRUCT) return ptr;
	return make_ld_ptr(ir, ptr, n->data_type);
}

uint32_t gen_lvalue_addr (IR *ir, ASTNode *lhs)
{
	if (lhs->type == NODE_INDEX) return gen_index_addr(ir, lhs);
	if (lhs->type == NODE_DOT) return gen_field_addr(ir, lhs);
	if (lhs->type != NODE_DEREF) return NO_REG;
	return gen_expr(ir, ir->ast->nodes + lhs->child);
}
