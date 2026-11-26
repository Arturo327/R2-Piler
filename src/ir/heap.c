#include "ir/ir.h"
#include "ir/internal.h"

static uint32_t libc_fn (IR *ir, int is_free)
{
	uint32_t *slot = is_free ? &ir->fn_free : &ir->fn_malloc;
	TypeId ret = is_free ? TYPE_VOID : type_ptr_to(TYPE_VOID);
	IRFn fn = {0};

	if (*slot) return *slot;
	fn.name = is_free ? "free" : "malloc";
	fn.len = (uint16_t)(is_free ? 4 : 6);
	fn.sym = fn.name;
	fn.ret_type = ret;
	fn.param_count = 1;
	fn.is_extern = 1;
	*slot = push_fn(ir, fn);
	return *slot;
}

static uint32_t new_dim_reg (IR *ir, ASTNode *n)
{
	uint32_t reg = gen_expr(ir, n);

	if (types[ir->reg_types[reg]].size < 8)
		reg = make_op(ir, IR_EXTEND, reg, NO_REG, TYPE_i64);
	return reg;
}

static uint32_t gen_new_ptr (IR *ir, ASTNode *n, uint32_t *lens)
{
	uint32_t custom = (n->flags & NODE_FLAG_CUSTOM_ALLOC) != 0;
	uint32_t dims = (uint32_t)n->u64;
	uint32_t node = custom ? ir->ast->nodes[n->child].next_bro : n->child;
	uint32_t ptr = NO_REG;
	uint32_t size = NO_REG;

	if (custom) ptr = gen_expr(ir, ir->ast->nodes + n->child);
	else size = make_const(ir, types[type_flat_element(n->data_type)].size, TYPE_i64);

	for (uint32_t d = 0; d < dims; d++) {
		lens[d] = new_dim_reg(ir, ir->ast->nodes + node);
		node = ir->ast->nodes[node].next_bro;
		if (!custom) size = make_op(ir, IR_MUL, size, lens[d], TYPE_i64);
	}
	if (custom) return ptr;

	make_arg(ir, size, 0);
	return make_call(ir, libc_fn(ir, 0), 1, type_ptr_to(TYPE_VOID));
}

uint32_t gen_new_desc (IR *ir, ASTNode *n)
{
	uint32_t idx = (uint32_t)(n - ir->ast->nodes);
	uint32_t dims = (uint32_t)n->u64;
	uint32_t lens[dims ? dims : 1];
	uint32_t off = 8;

	if (ir->desc_gen[idx] == ir->desc_epoch) return ir->desc[idx];
	uint32_t ptr = gen_new_ptr(ir, n, lens);
	uint32_t dst = new_struct_temp(ir, type_array_of(TYPE_u64, 1 + dims));
	make_str_ptr(ir, dst, ptr, type_ptr_to(type_flat_element(n->data_type)));
	for (uint32_t d = 0; d < dims; d++) {
		make_str_ptr(ir, add_offset(ir, dst, off), lens[d], TYPE_i64);
		off += 8;
	}
	ir->desc[idx] = dst;
	ir->desc_gen[idx] = ir->desc_epoch;
	return dst;
}

uint32_t gen_new (IR *ir, ASTNode *n)
{
	uint32_t idx = (uint32_t)(n - ir->ast->nodes);

	if (type_kind(n->data_type) == TK_SLICE)
		return make_ld_ptr(ir, gen_new_desc(ir, n),
				type_ptr_to(type_flat_element(n->data_type)));
	if (ir->desc_gen[idx] == ir->desc_epoch) return ir->desc[idx];

	uint32_t lens;
	uint32_t ptr = gen_new_ptr(ir, n, &lens);
	ir->desc[idx] = ptr;
	ir->desc_gen[idx] = ir->desc_epoch;
	return ptr;
}

uint32_t gen_new_ctor (IR *ir, ASTNode *n)
{
	uint32_t ptr;

	if (n->flags & NODE_FLAG_CUSTOM_ALLOC) {
		ptr = gen_expr(ir, ir->ast->nodes + (uint32_t)n->u64);
	} else {
		TypeId base = types[type_ptr_repr(n->data_type)].base;
		uint32_t size = make_const(ir, types[base].size, TYPE_i64);

		make_arg(ir, size, 0);
		ptr = make_call(ir, libc_fn(ir, 0), 1, type_ptr_to(TYPE_VOID));
	}
	return gen_ctor_call(ir, n, ptr);
}

void gen_delete (IR *ir, ASTNode *n)
{
	uint32_t child = n->child;

	if (n->flags & NODE_FLAG_CUSTOM_ALLOC) {
		TypeId sig = types[type_ptr_repr(ir->ast->nodes[child].data_type)].base;
		uint32_t callee = gen_expr(ir, ir->ast->nodes + child);

		child = ir->ast->nodes[child].next_bro;
		make_arg(ir, gen_expr(ir, ir->ast->nodes + child), 0);
		make_call_ind(ir, callee, sig, 1, TYPE_VOID);
		return;
	}
	make_arg(ir, gen_expr(ir, ir->ast->nodes + child), 0);
	make_call(ir, libc_fn(ir, 1), 1, TYPE_VOID);
}
