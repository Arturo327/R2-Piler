#include "ir/ir.h"
#include "ir/internal.h"

#include <assert.h>
#include <string.h>

#define LOGIC_BUDGET 8
#define COPY_UNROLL 8
#define SROA_MAX_LEAVES 8

static uint32_t gen_index_addr (IR *ir, ASTNode *n);
static uint32_t gen_array_addr (IR *ir, ASTNode *n);
static uint32_t read_reg (IR *ir, uint32_t reg, TypeId type);
static uint32_t gen_field_addr (IR *ir, ASTNode *n);
uint32_t add_offset (IR *ir, uint32_t base, uint64_t off);
static uint32_t gen_call (IR *ir, ASTNode *n, uint32_t dest);
static uint32_t gen_call_desc (IR *ir, ASTNode *n);
static uint32_t gen_call_value (IR *ir, ASTNode *n);
static uint32_t ptr_scale (IR *ir, uint32_t val, TypeId ptr_type, int up);
void gen_jump_if (IR *ir, ASTNode *n, uint32_t label, int when);

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

static uint32_t count_bros (IR *ir, uint32_t first)
{
	uint32_t count = 0;

	while (first != NO_NODE) {
		count++;
		first = ir->ast->nodes[first].next_bro;
	}
	return count;
}

static uint32_t gen_addr (IR *ir, ASTNode *n)
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

static uint32_t gen_deref (IR *ir, ASTNode *n)
{
	uint32_t ptr = gen_expr(ir, ir->ast->nodes + n->child);

	if (type_kind(n->data_type) == TK_STRUCT) return ptr;
	return make_ld_ptr(ir, ptr, n->data_type);
}

static uint32_t elem_scale (IR *ir, uint32_t val, TypeId elem)
{
	uint32_t size = types[elem].size;

	if (size < 2) return val;
	return make_op(ir, IR_MUL, val, make_const(ir, size, TYPE_i64), TYPE_i64);
}

static uint32_t slice_dim_reg (IR *ir, ASTNode *n, int dim, TypeId type)
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

static uint32_t push_dims (IR *ir, ASTNode *a, TypeId pt, uint32_t *regs, uint32_t k)
{
	TypeId t = pt;

	for (int d = 0; type_kind(t) == TK_ARRAY || type_kind(t) == TK_SLICE; d++) {
		if (type_kind(t) == TK_SLICE) regs[k++] = slice_dim_reg(ir, a, d, TYPE_i64);
		t = types[t].base;
	}
	return k;
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

static uint32_t gen_index_addr (IR *ir, ASTNode *n)
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

static uint32_t gen_array_addr (IR *ir, ASTNode *n)
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

static uint32_t read_reg (IR *ir, uint32_t reg, TypeId type)
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

static void store_string (IR *ir, uint32_t base, ASTNode *lit, TypeId elem)
{
	uint32_t zero = make_const(ir, 0, elem);

	for (uint16_t i = 0; i < lit->len; i++) {
		uint32_t c = make_const(ir, (uint8_t)lit->str[i], elem);

		make_str_ptr(ir, add_offset(ir, base, i), c, elem);
	}
	make_str_ptr(ir, add_offset(ir, base, lit->len), zero, elem);
}

static void zero_fill (IR *ir, uint32_t base, uint32_t from, uint32_t count, TypeId elem)
{
	uint32_t loop = ir->label_count++;
	uint32_t end = ir->label_count++;
	uint32_t i = new_reg(ir, TYPE_i64);
	uint32_t zero = make_const(ir, 0, elem);
	uint32_t one = make_const(ir, 1, TYPE_i64);
	uint32_t limit = make_const(ir, count, TYPE_i64);

	make_const_into(ir, i, from, TYPE_i64);
	make_label(ir, loop);
	make_jump(ir, IR_JNZ, make_op(ir, IR_GE, i, limit, TYPE_i64), end);
	make_str_ptr(ir, make_op(ir, IR_ADD, base, i, TYPE_i64), zero, elem);
	make_move(ir, i, make_op(ir, IR_ADD, i, one, TYPE_i64), TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, loop);
	make_label(ir, end);
}

void gen_array_init (IR *ir, uint32_t base, uint32_t init_idx, TypeId arr, int zero)
{
	ASTNode *init = ir->ast->nodes + init_idx;
	TypeId elem = types[arr].base;
	uint32_t elem_size = types[elem].size;

	if (init->type == NODE_LIT_STR) {
		store_string(ir, base, init, elem);
		if (zero && (uint32_t)init->len + 1 < types[arr].count)
			zero_fill(ir, base, (uint32_t)init->len + 1, types[arr].count, elem);
		return;
	}

	uint32_t i = 0;
	uint32_t item = init->child;

	while (item != NO_NODE) {
		if (type_kind(elem) == TK_ARRAY) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_array_init(ir, at, item, elem, zero);
		} else if (type_kind(elem) == TK_STRUCT) {
			uint32_t at = add_offset(ir, base, (uint64_t)i * elem_size);

			gen_struct_into(ir, at, item, elem, 1);
		} else {
			uint32_t val = gen_expr(ir, ir->ast->nodes + item);

			make_str_ptr(ir, add_offset(ir, base, (uint64_t)i * elem_size), val, elem);
		}
		item = ir->ast->nodes[item].next_bro;
		i++;
	}
}

static uint32_t gen_lvalue_addr (IR *ir, ASTNode *lhs)
{
	if (lhs->type == NODE_INDEX) return gen_index_addr(ir, lhs);
	if (lhs->type == NODE_DOT) return gen_field_addr(ir, lhs);
	if (lhs->type != NODE_DEREF) return NO_REG;
	return gen_expr(ir, ir->ast->nodes + lhs->child);
}

static uint32_t load_var (IR *ir, Symbol *s)
{
	if (!s->depth || s->is_static)
		return make_ld_global(ir, s->ir_id, s->type);

	uint32_t tmp = new_reg(ir, s->type);
	make_move(ir, tmp, s->ir_id, s->type);
	return tmp;
}

static uint32_t new_struct_temp (IR *ir, TypeId type)
{
	uint32_t reg = new_reg(ir, type);
	uint32_t dst = new_reg(ir, TYPE_i64);

	ir->addressed[reg] = 1;
	ir->no_tail = 1;
	make_addr_local(ir, dst, reg, type);
	return dst;
}

static void gen_copy_loop (IR *ir, uint32_t dst, uint32_t src, uint32_t size)
{
	uint32_t loop = ir->label_count++;
	uint32_t end = ir->label_count++;
	uint32_t off = new_reg(ir, TYPE_i64);
	uint32_t limit = make_const(ir, size, TYPE_i64);
	uint32_t step = make_const(ir, 8, TYPE_i64);

	make_const_into(ir, off, 0, TYPE_i64);
	make_label(ir, loop);
	make_jump(ir, IR_JNZ, make_op(ir, IR_GE, off, limit, TYPE_i64), end);

	uint32_t val = make_ld_ptr(ir, make_op(ir, IR_ADD, src, off, TYPE_i64), TYPE_u64);
	make_str_ptr(ir, make_op(ir, IR_ADD, dst, off, TYPE_i64), val, TYPE_u64);
	make_move(ir, off, make_op(ir, IR_ADD, off, step, TYPE_i64), TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, loop);
	make_label(ir, end);
}

static void gen_copy_fields (IR *ir, uint32_t dst, uint32_t src, TypeId type, uint32_t base)
{
	const Type *t = types + type;

	for (uint16_t i = 0; i < t->field_count; i++) {
		const StructField *f = t->fields + i;
		uint32_t off = base + f->offset;
		uint32_t val;

		if (type_kind(f->type) == TK_STRUCT) {
			gen_copy_fields(ir, dst, src, f->type, off);
			continue;
		}
		val = make_ld_ptr(ir, add_offset(ir, src, off), f->type);
		make_str_ptr(ir, add_offset(ir, dst, off), val, f->type);
	}
}

static void gen_copy (IR *ir, uint32_t dst, uint32_t src, TypeId type)
{
	uint32_t size = types[type].size;
	uint32_t off = 0;
	int budget = SROA_MAX_LEAVES;

	if (type_scalar_leaves(type, &budget)) {
		gen_copy_fields(ir, dst, src, type, 0);
		return;
	}

	if ((size >> 3) > COPY_UNROLL) {
		off = size & ~7u;
		gen_copy_loop(ir, dst, src, off);
	}
	while (off < size) {
		uint32_t left = size - off;
		TypeId ut = left >= 8 ? TYPE_u64 : left >= 4 ? TYPE_u32
				: left >= 2 ? TYPE_u16 : TYPE_u8;
		uint32_t val = make_ld_ptr(ir, add_offset(ir, src, off), ut);

		make_str_ptr(ir, add_offset(ir, dst, off), val, ut);
		off += types[ut].size;
	}
}

static uint32_t gen_field_addr (IR *ir, ASTNode *n)
{
	ASTNode *left = ir->ast->nodes + n->child;
	const StructField *f = find_field(left->data_type, n->str, n->len);
	uint32_t base = gen_expr(ir, left);

	return add_offset(ir, base, f->offset);
}

static uint32_t gen_field (IR *ir, ASTNode *n)
{
	uint32_t addr = gen_field_addr(ir, n);

	if (type_kind(n->data_type) == TK_STRUCT)
		return addr;
	if (type_dyn_rank(n->data_type) > 0)
		return make_ld_ptr(ir, addr,
				type_ptr_to(type_flat_element(n->data_type)));
	if (type_kind(n->data_type) == TK_ARRAY)
		return addr;
	return make_ld_ptr(ir, addr, n->data_type);
}

uint32_t gen_slice_desc (IR *ir, uint32_t dst, ASTNode *val, TypeId type)
{
	TypeId ptr_type = type_ptr_to(type_flat_element(type));

	if (val->type == NODE_FN_CALL && type_dyn_rank(val->data_type) > 0) {
		uint32_t buf = gen_call_desc(ir, val);

		gen_copy(ir, dst, buf, val->data_type);
		return make_ld_ptr(ir, buf, ptr_type);
	}

	uint32_t src_ptr = gen_expr(ir, val);
	make_str_ptr(ir, dst, src_ptr, ptr_type);

	int slice_index = 0;
	TypeId level_type = type;
	int level = 0;

	while (type_kind(level_type) == TK_ARRAY || type_kind(level_type) == TK_SLICE) {
		if (type_kind(level_type) == TK_SLICE) {
			uint32_t length = slice_dim_reg(ir, val, level, TYPE_i64);
			make_str_ptr(ir, add_offset(ir, dst, (uint64_t)8 * (slice_index + 1)), length, TYPE_i64);
			slice_index++;
		}
		level_type = types[level_type].base;
		level++;
	}
	return src_ptr;
}

static void gen_struct_init (IR *ir, uint32_t base, ASTNode *init, TypeId type)
{
	uint32_t item = init->child;

	while (item != NO_NODE) {
		ASTNode *fi = ir->ast->nodes + item;
		const StructField *f = find_field(type, fi->str, fi->len);
		uint32_t at = add_offset(ir, base, f->offset);

		if (type_kind(f->type) == TK_STRUCT)
			gen_struct_into(ir, at, fi->child, f->type, 1);
		else if (type_dyn_rank(f->type) > 0) {
			ASTNode *val = ir->ast->nodes + fi->child;
			gen_slice_desc(ir, at, val, f->type);
		} else if (type_kind(f->type) == TK_ARRAY)
			gen_array_init(ir, at, fi->child, f->type, 1);
		else make_str_ptr(ir, at, gen_expr(ir, ir->ast->nodes + fi->child), f->type);
		item = fi->next_bro;
	}
}

void gen_struct_into (IR *ir, uint32_t dst, uint32_t expr, TypeId type, int fresh)
{
	ASTNode *e = ir->ast->nodes + expr;

	if (e->type == NODE_STRUCT_INIT) {
		gen_struct_init(ir, dst, e, type);
		return;
	}
	if (e->type == NODE_FN_CALL && fresh
			&& ret_count(ir, call_ret_node(ir, e)) == 1) {
		gen_call(ir, e, dst);
		return;
	}
	gen_copy(ir, dst, gen_expr(ir, e), type);
}

static int args_assign (IR *ir, uint32_t idx)
{
	ASTNode *n = ir->ast->nodes + idx;

	if (n->type >= NODE_ASSIGN && n->type <= NODE_RS_ASSIGN) return 1;
	for (uint32_t k = n->child; k != NO_NODE; k = ir->ast->nodes[k].next_bro) {
		if (args_assign(ir, k)) return 1;
	}
	return 0;
}

static int can_alias_arg (IR *ir, ASTNode *call, uint32_t arg, uint32_t param)
{
	uint32_t sym = agg_root_sym(ir->ast, arg);

	if (sym == NO_SYMBOL || param == NO_NODE) return 0;
	Symbol *s = ir->symtab->symbols + sym;
	Symbol *ps = ir->symtab->symbols + ir->ast->nodes[param].sym;

	if (s->kind != SYMBOL_VAR || !s->depth || s->is_static || s->escaped) return 0;
	if (ps->written || ps->escaped) return 0;

	for (uint32_t k = call->child; k != NO_NODE; k = ir->ast->nodes[k].next_bro) {
		if (args_assign(ir, k)) return 0;
	}

	int budget = SROA_MAX_LEAVES;
	if (type_scalar_leaves(s->type, &budget)) return 0;

	return 1;
}

static uint32_t gen_struct_arg (IR *ir, ASTNode *a, int alias)
{
	uint32_t src = gen_expr(ir, a);
	if (alias) ir->no_tail = 1;
	if (a->type == NODE_FN_CALL || alias) return src;

	uint32_t tmp = new_struct_temp(ir, a->data_type);
	gen_copy(ir, tmp, src, a->data_type);
	return tmp;
}

static uint32_t gen_call (IR *ir, ASTNode *n, uint32_t dest)
{
	Symbol *fn = ir->symtab->symbols + n->sym;
	uint32_t ret = call_ret_node(ir, n);
	int sret = fn_uses_sret(ir, ret);
	uint32_t argc = count_bros(ir, n->child);
	uint32_t regs[ir->fns[fn->ir_id].param_count + 1];
	uint32_t params = ir->ast->nodes[fn->decl].child;
	uint32_t param = ir->ast->nodes[params].child;
	uint32_t arg = n->child;
	uint32_t k = 0;

	if (sret) regs[k++] = dest != NO_REG ? dest : new_struct_temp(ir, ret_buf_type(ir, ret));
	for (uint32_t i = 0; i < argc; i++) {
		ASTNode *a = ir->ast->nodes + arg;
		TypeId pt = param == NO_NODE ? TYPE_VOID : ir->ast->nodes[param].data_type;

		regs[k++] = type_kind(pt) == TK_STRUCT
				? gen_struct_arg(ir, a, can_alias_arg(ir, n, arg, param))
				: gen_expr(ir, a);

		k = push_dims(ir, a, pt, regs, k);
		arg = a->next_bro;
		if (param != NO_NODE) param = ir->ast->nodes[param].next_bro;
	}

	for (uint32_t i = 0; i < k; i++)
		make_arg(ir, regs[i], i);

	if (sret) {
		make_call(ir, fn->ir_id, (uint16_t)k, TYPE_VOID);
		return regs[0];
	}
	return make_call(ir, fn->ir_id, (uint16_t)k, ir->fns[fn->ir_id].ret_type);
}

static uint32_t gen_call_desc (IR *ir, ASTNode *n)
{
	uint32_t idx = (uint32_t)(n - ir->ast->nodes);

	if (ir->desc[idx] == NO_REG) ir->desc[idx] = gen_call(ir, n, NO_REG);
	return ir->desc[idx];
}

static uint32_t gen_call_value (IR *ir, ASTNode *n)
{
	if (!fn_uses_sret(ir, call_ret_node(ir, n))) return gen_call(ir, n, NO_REG);
	TypeId type = n->data_type;

	if (type_kind(type) == TK_STRUCT) return gen_call(ir, n, NO_REG);
	if (type_dyn_rank(type) > 0)
		return make_ld_ptr(ir, gen_call_desc(ir, n),
				type_ptr_to(type_flat_element(type)));

	uint32_t buf = gen_call(ir, n, NO_REG);
	if (type_kind(type) == TK_ARRAY)
		return make_ld_ptr(ir, buf, type_ptr_to(type_flat_element(type)));
	return make_ld_ptr(ir, buf, type);
}

static uint32_t gen_assign (IR *ir, ASTNode *n)
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

	if (type_kind(lhs->data_type) == TK_STRUCT) {
		uint32_t dst = gen_expr(ir, lhs);

		gen_struct_into(ir, dst, lhs->next_bro, lhs->data_type, 0);
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

static uint32_t gen_compound_assign (IR *ir, ASTNode *n)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	ASTNode *rhs = ir->ast->nodes + lhs->next_bro;
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

static uint32_t gen_unary (IR *ir, ASTNode *n)
{
	ASTNode *operand = ir->ast->nodes + n->child;
	uint32_t a = gen_expr(ir, operand);

	return make_op(ir, node_to_ir[n->type], a, NO_REG, operand->data_type);
}

static uint32_t ptr_scale (IR *ir, uint32_t val, TypeId ptr_type, int up)
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
			|| type_kind(operand->data_type) == TK_SLICE)
		return src;
	if (types[n->data_type].size == 8 && types[operand->data_type].size == 8) {
		ir->reg_types[src] = n->data_type;
		return src;
	}
	if (n->data_type == operand->data_type) return src;

	return make_op(ir, IR_EXTEND, src, NO_REG, n->data_type);
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

	default:
		assert(node_to_ir[n->type] != IR_NOP);
		return gen_binary(ir, n);
	}
}

uint32_t local_storage_addr (IR *ir, Symbol *s, TypeId type)
{
	if ((uint32_t)(s - ir->symtab->symbols) == ir->nrvo) return ir->sret;
	uint32_t dst = new_reg(ir, TYPE_i64);

	ir->addressed[s->ir_id] = 1;
	make_addr_local(ir, dst, s->ir_id, type);
	return dst;
}

int struct_is_small (TypeId type)
{
	int budget = SROA_MAX_LEAVES;
	return type_scalar_leaves(type, &budget);
}
