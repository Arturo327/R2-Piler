#include "sema/common.h"

#define MAX_EXPR_DEPTH 5000

static uint8_t check_literal (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	switch (n->type)
	{
	case NODE_LIT_i64:
		n->data_type = n->u64 > INT64_MAX ? TYPE_UNTYPED_UINT : TYPE_UNTYPED_INT;
		break;
	case NODE_LIT_u64: n->data_type = TYPE_UNTYPED_UINT; break;
	case NODE_LIT_CHAR: n->data_type = TYPE_UNTYPED_CHAR; break;
	default:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"string literals are not supported yet");
		n->data_type = TYPE_ERROR;
		break;
	}
	return n->data_type;
}

static uint8_t check_init_ref (Sema *s, uint32_t idx, uint32_t sym_idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	Symbol *sym = &s->table.symbols[sym_idx];

	if (sym->decl == s->init_node) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' cannot be used in its own initializer", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (sym->depth != 0) return sym->type;

	if (sym->state == INIT_PENDING)
		check_global_var_init(s, sym->decl);

	if (sym->state != INIT_CHECKING) return sym->type;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"'%.*s' cannot be used in a circular initialization", (int)n->len, n->str);
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

static uint8_t check_id (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);

	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'%.*s' is not declared",
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (s->table.symbols[sym_idx].kind == SYMBOL_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is a function, not a variable", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->sym = sym_idx;

	if (s->init_node != NO_NODE
			&& s->table.symbols[sym_idx].kind == SYMBOL_VAR) {
		if (check_init_ref(s, idx, sym_idx) == TYPE_ERROR) return TYPE_ERROR;
	}

	if (!s->table.symbols[sym_idx].assigned)
		error_report(s->err, ERR_WARNING, node_loc(n),
				"'%.*s' is used without being assigned", (int)n->len, n->str);

	n->data_type = s->table.symbols[sym_idx].type;
	return n->data_type;
}

static uint32_t adapt_call_arg (Sema *s, uint32_t arg, uint8_t arg_type, uint8_t param_type)
{
	int ok;
	uint32_t new_arg = force_cast(s, arg, arg_type, param_type, &ok);

	if (ok) return new_arg;

	const char *hint = arg_type == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[new_arg]),
			"argument type %s does not match parameter type %s; %s",
			types[arg_type].name, types[param_type].name, hint);
	return new_arg;
}

static void check_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx)
{
	ASTNode *call = &s->ast->nodes[call_idx];
	uint32_t params_node = s->ast->nodes[fn_idx].child;
	uint32_t param = s->ast->nodes[params_node].child;
	uint32_t arg = call->child;
	uint32_t prev = NO_NODE;
	int count_ok = 1;

	while (arg != NO_NODE) {
		uint8_t arg_type = check_expr(s, arg);
		uint8_t param_type = param != NO_NODE ? s->ast->nodes[param].data_type : TYPE_VOID;
		uint32_t next = s->ast->nodes[arg].next_bro;

		if (param == NO_NODE) {
			count_ok = 0;
		} else if (arg_type != TYPE_ERROR && param_type != TYPE_ERROR
				&& arg_type != param_type) {
			arg = adapt_call_arg(s, arg, arg_type, param_type);
			if (prev == NO_NODE) call->child = arg;
			else s->ast->nodes[prev].next_bro = arg;
		}

		prev = arg;
		if (param != NO_NODE) param = s->ast->nodes[param].next_bro;
		arg = next;
	}

	if (param != NO_NODE) count_ok = 0;

	if (!count_ok) error_report(s->err, ERR_ERROR, node_loc(call),
			"wrong number of arguments in call to '%.*s'",
			(int)call->len, call->str);
}

static uint8_t check_fn_call (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);

	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'%.*s' is not a known function",
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (s->table.symbols[sym_idx].kind != SYMBOL_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is a variable, not a function", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->sym = sym_idx;
	Symbol *fn = &s->table.symbols[sym_idx];
	check_call_args(s, idx, fn->decl);
	n->data_type = fn->type;
	return n->data_type;
}

static uint8_t check_assign_target (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);

	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'%.*s' is not declared",
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (s->table.symbols[sym_idx].kind == SYMBOL_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is a function, not a variable", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->sym = sym_idx;

	if (s->init_node != NO_NODE
			&& s->table.symbols[sym_idx].kind == SYMBOL_VAR) {
		if (check_init_ref(s, idx, sym_idx) == TYPE_ERROR) return TYPE_ERROR;
	}

	n->data_type = s->table.symbols[sym_idx].type;
	return n->data_type;
}

static uint8_t check_assign (Sema *s, uint32_t idx)
{
	ASTNode *assign = &s->ast->nodes[idx];
	uint32_t left = assign->child;
	ASTNode *left_node = &s->ast->nodes[left];
	uint32_t right = left_node->next_bro;

	if (left_node->type != NODE_ID) {
		error_report(s->err, ERR_ERROR, node_loc(assign),
				"left side of '=' must be a variable");
		check_expr(s, right);
		assign->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	uint8_t l = check_assign_target(s, left);
	uint8_t r = check_expr(s, right);

	if (left_node->sym != NO_SYMBOL)
		s->table.symbols[left_node->sym].assigned = 1;

	if (l == TYPE_ERROR || r == TYPE_ERROR) {
		assign->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (l != r) {
		int ok;

		right = force_cast(s, right, r, l, &ok);
		left_node->next_bro = right;

		if (!ok) {
			const char *hint = r == TYPE_VOID
					? "a void value cannot be used here"
					: "use an explicit cast with 'as'";

			error_report(s->err, ERR_ERROR, node_loc(assign),
					"cannot assign %s to a variable of type %s; %s",
					types[r].name, types[l].name, hint);

			assign->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}

	assign->data_type = l;
	return l;
}

static int const_operand (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	if (n->type == NODE_LIT_i64 || n->type == NODE_LIT_u64 || n->type == NODE_LIT_CHAR)
		return 1;
	if (n->type == NODE_CAST)
		return const_operand(s, n->child);
	return 0;
}

static uint8_t check_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t op = n->type;
	uint8_t operand_type = check_expr(s, n->child);
	uint8_t result_type;
	int is_lit;

	if (operand_type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (operand_type == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator cannot be applied to a value of type %s",
				types[operand_type].name);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = (n->type == NODE_NOT_L) ? TYPE_i64 : operand_type;
	is_lit = const_operand(s, n->child);
	result_type = try_fold_unary(s, idx);

	if (op == NODE_NEG && !types[result_type].sign && !is_lit)
		error_report(s->err, ERR_WARNING, node_loc(n),
				"negating an unsigned value; the result wraps");

	n->data_type = result_type;
	return result_type;
}

static uint8_t binop_result_type (uint8_t node_type, uint8_t operand_type)
{
	switch (node_type)
	{
	case NODE_EQ: case NODE_NE: case NODE_GT: case NODE_GE:
	case NODE_LT: case NODE_LE: case NODE_AND_L: case NODE_OR_L:
		return TYPE_i64;
	default:
		return operand_type;
	}
}

static int needs_common_type (uint8_t node_type)
{
	switch (node_type)
	{
	case NODE_AND_L: case NODE_OR_L: case NODE_LS: case NODE_RS:
		return 0;
	default:
		return 1;
	}
}

static void warn_const_operand (Sema *s, uint32_t idx, const char *op)
{
	ASTNode *n = s->ast->nodes + idx;
	int tru;

	if (n->type == NODE_CAST) {
		warn_const_operand(s, n->child, op);
		return;
	}

	switch (n->type)
	{
	case NODE_LIT_i64: tru = n->i64 != 0; break;
	case NODE_LIT_u64: tru = n->u64 != 0; break;
	case NODE_LIT_CHAR: tru = n->chr != 0; break;
	default: return;
	}

	error_report(s->err, ERR_WARNING, node_loc(n),
			"constant operand of '%s' is always %s", op, tru ? "true" : "false");
}

static uint8_t check_binary (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t left = n->child;
	uint32_t right = s->ast->nodes[left].next_bro;
	uint8_t l = check_expr(s, left);
	uint8_t r = check_expr(s, right);

	if (l == TYPE_ERROR || r == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (l == TYPE_VOID || r == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator cannot be applied to a value of type void");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (n->type == NODE_AND_L || n->type == NODE_OR_L) {
		const char *op = n->type == NODE_AND_L ? "&&" : "||";
		warn_const_operand(s, left, op);
		warn_const_operand(s, right, op);
		finalize_type(s, left, l);
		finalize_type(s, right, r);
	} else if (n->type == NODE_LS || n->type == NODE_RS) {
		finalize_type(s, right, r);
		if (!is_literal_node(s, right))
			l = finalize_type(s, left, l);
	}

	if (l != r && needs_common_type(n->type)
			&& !unify_operands(s, n, &left, &right, &l, &r)) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->child = left;
	s->ast->nodes[left].next_bro = right;
	n->data_type = binop_result_type(n->type, l);
	try_fold_binary(s, idx);
	return n->data_type;
}

static uint8_t check_cast (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t from = check_expr(s, n->child);

	if (from == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (from == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast a value of type void");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	finalize_type(s, n->child, from);
	return n->data_type;
}

static uint8_t check_expr_node (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64:
	case NODE_LIT_CHAR: case NODE_LIT_STR:
		return check_literal(s, idx);
	case NODE_ID: return check_id(s, idx);
	case NODE_FN_CALL: return check_fn_call(s, idx);
	case NODE_ASSIGN: return check_assign(s, idx);
	case NODE_NEG: case NODE_NOT_L: case NODE_NOT_A:
		return check_unary(s, idx);
	case NODE_CAST: return check_cast(s, idx);
	case NODE_EMPTY: n->data_type = TYPE_VOID; return TYPE_VOID;
	case NODE_ERROR: n->data_type=TYPE_ERROR; return TYPE_ERROR;
	default: return check_binary(s, idx);
	}
}

uint8_t check_expr (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint8_t type;

	if (s->expr_depth >= MAX_EXPR_DEPTH) {
		if (!s->too_deep)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"expression or initializer chain nested too deeply");
		s->too_deep = 1;
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	s->expr_depth++;
	type = check_expr_node(s, idx);
	s->expr_depth--;
	return type;
}
