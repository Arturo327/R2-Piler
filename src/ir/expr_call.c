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

static uint32_t push_arg (IR *ir, ASTNode *call, uint32_t arg, uint32_t param, uint32_t *regs, uint32_t k)
{
	ASTNode *a = ir->ast->nodes + arg;
	TypeId pt = param == NO_NODE ? TYPE_VOID : ir->ast->nodes[param].data_type;
	uint32_t n8 = struct_eightbytes(pt);

	if (n8) {
		pack_struct(ir, gen_expr(ir, a), pt, regs + k);
		return k + n8;
	}
	regs[k++] = type_kind(pt) == TK_STRUCT
			? gen_struct_arg(ir, a, can_alias_arg(ir, call, arg, param))
			: gen_expr(ir, a);
	return push_dims(ir, a, pt, regs, k);
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

uint32_t gen_call (IR *ir, ASTNode *n, uint32_t dest)
{
	Symbol *fn = ir->symtab->symbols + n->sym;
	uint32_t ret = call_ret_node(ir, n);
	TypeId rtype = ir->ast->nodes[ret].data_type;
	int sret = fn_uses_sret(ir, ret);
	uint32_t argc = count_bros(ir, n->child);
	uint32_t regs[ir->fns[fn->ir_id].param_count + 1];
	uint32_t params = ir->ast->nodes[fn->decl].child;
	uint32_t param = ir->ast->nodes[params].child;
	uint32_t arg = n->child;
	uint32_t k = 0;

	if (sret) regs[k++] = dest != NO_REG ? dest : new_struct_temp(ir, ret_buf_type(ir, ret));
	for (uint32_t i = 0; i < argc; i++) {
		k = push_arg(ir, n, arg, param, regs, k);
		arg = ir->ast->nodes[arg].next_bro;
		if (param != NO_NODE) param = ir->ast->nodes[param].next_bro;
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

uint32_t gen_call_desc (IR *ir, ASTNode *n)
{
	uint32_t idx = (uint32_t)(n - ir->ast->nodes);

	if (ir->desc[idx] == NO_REG) ir->desc[idx] = gen_call(ir, n, NO_REG);
	return ir->desc[idx];
}

uint32_t gen_call_value (IR *ir, ASTNode *n)
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
