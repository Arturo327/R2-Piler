#include "ir/ir.h"
#include "ir/internal.h"

static uint32_t count_bros (IR *ir, uint32_t first)
{
	uint32_t count = 0;

	while (first != NO_NODE) {
		count++;
		first = ir->ast->nodes[first].next_bro;
	}
	return count;
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
	uint32_t psym = ir->ast->nodes[param].sym;

	if (psym == NO_SYMBOL) return 0;
	Symbol *s = ir->symtab->symbols + sym;
	Symbol *ps = ir->symtab->symbols + psym;

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
	if (a->type == NODE_CAST && (a->flags & NODE_FLAG_LITERAL) && a->child != NO_NODE) {
		ASTNode *init = ir->ast->nodes + a->child;

		if (init->type == NODE_STRUCT_INIT) {
			uint32_t tmp = new_struct_temp(ir, a->data_type);

			gen_struct_init(ir, tmp, init, a->data_type);
			return tmp;
		}
	}

	uint32_t src = gen_expr(ir, a);
	if (alias) ir->no_tail = 1;
	if (a->type == NODE_FN_CALL || alias) return src;

	uint32_t tmp = new_struct_temp(ir, a->data_type);
	gen_copy(ir, tmp, src, a->data_type);
	return tmp;
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

static uint32_t push_arg_t (IR *ir, ASTNode *call, uint32_t arg, TypeId pt,
		uint32_t param, uint32_t *regs, uint32_t k)
{
	ASTNode *a = ir->ast->nodes + arg;
	uint32_t n8 = struct_eightbytes(pt);

	if (n8) {
		pack_struct(ir, gen_expr(ir, a), pt, regs + k);
		return k + n8;
	}
	regs[k++] = type_is_struct_like(pt)
			? gen_struct_arg(ir, a, param != NO_NODE && can_alias_arg(ir, call, arg, param))
			: gen_expr(ir, a);
	return push_dims(ir, a, pt, regs, k);
}

static uint32_t push_arg (IR *ir, ASTNode *call, uint32_t arg, uint32_t param, uint32_t *regs, uint32_t k)
{
	TypeId pt = param == NO_NODE ? TYPE_VOID : ir->ast->nodes[param].data_type;

	return push_arg_t(ir, call, arg, pt, param, regs, k);
}

static uint32_t call_struct_regs (IR *ir, uint32_t fn_idx, uint32_t argc, TypeId type, uint32_t dest)
{
	uint32_t words[2] = { NO_REG, NO_REG };

	words[0] = make_call(ir, fn_idx, (uint16_t)argc, TYPE_u64);
	if (struct_eightbytes(type) == 2) words[1] = make_rethi(ir);
	if (dest == NO_REG) dest = new_struct_temp(ir, type);
	unpack_struct(ir, dest, type, words);
	return dest;
}

static uint32_t push_spread (IR *ir, uint32_t arg, uint32_t *regs, uint32_t k)
{
	ASTNode *n = ir->ast->nodes + arg;
	ASTNode *src = ir->ast->nodes + n->child;

	regs[k++] = gen_expr(ir, src);
	regs[k++] = slice_dim_reg(ir, src, 0, TYPE_i64);
	return k;
}

static uint32_t push_varargs (IR *ir, uint32_t arg, TypeId slice, uint32_t *regs, uint32_t k)
{
	TypeId elem = types[slice].base;
	TypeKind kind = type_kind(elem);
	uint32_t count = count_bros(ir, arg);

	if (arg != NO_NODE && ir->ast->nodes[arg].type == NODE_SPREAD)
		return push_spread(ir, arg, regs, k);
	if (!count) {
		regs[k++] = make_const(ir, 0, type_ptr_to(elem));
		regs[k++] = make_const(ir, 0, TYPE_i64);
		return k;
	}
	uint32_t base = new_struct_temp(ir, type_array_of(elem, count));
	uint32_t at = base;

	while (arg != NO_NODE) {
		ASTNode *e = ir->ast->nodes + arg;

		if ((kind == TK_STRUCT || kind == TK_CLASS)) gen_struct_into(ir, at, arg, elem, 1);
		else if (kind == TK_ARRAY) gen_copy(ir, at, gen_expr(ir, e), elem);
		else make_str_ptr(ir, at, gen_expr(ir, e), elem);
		at = add_offset(ir, at, types[elem].size);

		arg = ir->ast->nodes[arg].next_bro;
	}
	regs[k++] = base;
	regs[k++] = make_const(ir, count, TYPE_i64);
	return k;
}

static uint32_t make_ind_call (IR *ir, ASTNode *n, TypeId rtype, uint32_t callee,
		uint32_t argc, uint32_t dest)
{
	TypeId sig = types[n->sym].base;

	if (struct_eightbytes(rtype)) {
		uint32_t words[2] = { NO_REG, NO_REG };

		words[0] = make_call_ind(ir, callee, sig, (uint16_t)argc, TYPE_u64);
		if (struct_eightbytes(rtype) == 2) words[1] = make_rethi(ir);
		if (dest == NO_REG) dest = new_struct_temp(ir, rtype);
		unpack_struct(ir, dest, rtype, words);
		return dest;
	}
	return make_call_ind(ir, callee, sig, (uint16_t)argc, call_ret_abi(ir, n));
}

static uint32_t gen_call_ind (IR *ir, ASTNode *n, uint32_t dest)
{
	const Type *sig = types + types[n->sym].base;
	uint32_t argc = count_bros(ir, n->child);
	uint32_t regs[call_param_slots(ir, n) + argc + 2];
	uint32_t sret = (uint32_t)call_uses_sret(ir, n);
	uint32_t arg = n->child;
	uint32_t k = 0;
	uint32_t i = 0;

	if (sret) regs[k++] = dest != NO_REG ? dest : new_struct_temp(ir, call_buf_type(ir, n));
	for (; i < argc; i++) {
		if (i < sig->param_count)
			k = push_arg_t(ir, n, arg, sig->params[i], NO_NODE, regs, k);
		else regs[k++] = gen_expr(ir, ir->ast->nodes + arg);
		arg = ir->ast->nodes[arg].next_bro;
	}

	uint32_t callee = gen_expr(ir, ir->ast->nodes + (uint32_t)n->u64);
	for (uint32_t j = 0; j < k; j++)
		make_arg(ir, regs[j], j);
	if (sret) {
		make_call_ind(ir, callee, types[n->sym].base, (uint16_t)k, TYPE_VOID);
		return regs[0];
	}
	return make_ind_call(ir, n, call_ret_type(ir, n, 0), callee, k, dest);
}

static uint32_t push_typed_args (IR *ir, ASTNode *n, uint32_t arg, uint32_t param,
		uint32_t *regs, uint32_t k)
{
	while (param != NO_NODE && ir->ast->nodes[param].next_bro != NO_NODE) {
		k = push_arg(ir, n, arg, param, regs, k);
		arg = ir->ast->nodes[arg].next_bro;
		param = ir->ast->nodes[param].next_bro;
	}
	return push_varargs(ir, arg, ir->ast->nodes[param].data_type, regs, k);
}

uint32_t gen_call (IR *ir, ASTNode *n, uint32_t dest)
{
	if (n->flags & NODE_FLAG_INDIRECT) return gen_call_ind(ir, n, dest);

	Symbol *fn = ir->symtab->symbols + n->sym;
	uint32_t ret = call_ret_node(ir, n);
	TypeId rtype = ir->ast->nodes[ret].data_type;
	int sret = fn_uses_sret(ir, ret);
	uint32_t argc = count_bros(ir, n->child);
	uint32_t regs[ir->fns[fn->ir_id].param_count + argc + 1];
	uint32_t params = ir->ast->nodes[fn->decl].child;
	uint32_t param = ir->ast->nodes[params].child;
	uint32_t arg = n->child;
	uint32_t k = 0;

	if (sret) regs[k++] = dest != NO_REG ? dest : new_struct_temp(ir, ret_buf_type(ir, ret));
	if (ir->ast->nodes[params].flags & NODE_FLAG_VARARG_TYPED) {
		k = push_typed_args(ir, n, arg, param, regs, k);
	} else {
		for (uint32_t i = 0; i < argc; i++) {
			k = push_arg(ir, n, arg, param, regs, k);
			arg = ir->ast->nodes[arg].next_bro;
			if (param != NO_NODE) param = ir->ast->nodes[param].next_bro;
		}
	}

	for (uint32_t i = 0; i < k; i++)
		make_arg(ir, regs[i], i);

	if (sret) {
		make_call(ir, fn->ir_id, (uint16_t)k, TYPE_VOID);
		return regs[0];
	}
	if (struct_eightbytes(rtype))
		return call_struct_regs(ir, fn->ir_id, k, rtype, dest);
	return make_call(ir, fn->ir_id, (uint16_t)k, ir->fns[fn->ir_id].ret_type);
}

uint32_t gen_ctor_call (IR *ir, ASTNode *n, uint32_t dst)
{
	if (n->sym == NO_SYMBOL) return dst;

	Symbol *fn = ir->symtab->symbols + n->sym;
	uint32_t params = ir->ast->nodes[fn->decl].child;
	uint32_t param = ir->ast->nodes[params].child;
	uint32_t arg = n->child;
	uint32_t argc = count_bros(ir, arg);
	uint32_t regs[ir->fns[fn->ir_id].param_count + argc + 2];
	uint32_t k = 0;

	if (param != NO_NODE && (ir->ast->nodes[param].flags & NODE_FLAG_SELF))
		param = ir->ast->nodes[param].next_bro;
	regs[k++] = dst;

	if (ir->ast->nodes[params].flags & NODE_FLAG_VARARG_TYPED) {
		k = push_typed_args(ir, n, arg, param, regs, k);
	} else {
		for (uint32_t i = 0; i < argc; i++) {
			k = push_arg(ir, n, arg, param, regs, k);
			arg = ir->ast->nodes[arg].next_bro;
			if (param != NO_NODE) param = ir->ast->nodes[param].next_bro;
		}
	}

	for (uint32_t i = 0; i < k; i++)
		make_arg(ir, regs[i], i);
	make_call(ir, fn->ir_id, (uint16_t)k, TYPE_VOID);
	return dst;
}

uint32_t gen_call_desc (IR *ir, ASTNode *n)
{
	uint32_t idx = (uint32_t)(n - ir->ast->nodes);

	if (ir->desc_gen[idx] != ir->desc_epoch) {
		ir->desc[idx] = gen_call(ir, n, NO_REG);
		ir->desc_gen[idx] = ir->desc_epoch;
	}
	return ir->desc[idx];
}

uint32_t gen_call_value (IR *ir, ASTNode *n)
{
	if (!call_uses_sret(ir, n)) return gen_call(ir, n, NO_REG);
	TypeId type = n->data_type;

	if (type_is_struct_like(type)) return gen_call(ir, n, NO_REG);
	if (type_dyn_rank(type) > 0)
		return make_ld_ptr(ir, gen_call_desc(ir, n),
				type_ptr_to(type_flat_element(type)));

	uint32_t buf = gen_call(ir, n, NO_REG);
	if (type_kind(type) == TK_ARRAY)
		return make_ld_ptr(ir, buf, type_ptr_to(type_flat_element(type)));
	return make_ld_ptr(ir, buf, type);
}
