#include "sema/sema.h"

#include <stdio.h>
#include <string.h>

#define RESERVED_PREFIX "__r2_"
#define RESERVED_PREFIX_LEN (sizeof(RESERVED_PREFIX) - 1)
#define MAX_PARAMS 255

static ErrorLoc node_loc (ASTNode *n)
{
	ErrorLoc loc = {
		.line = n->line,
		.col = n->col,
		.len = n->len ? n->len : 1
	};
	return loc;
}

void sema_init (Sema *s, Arena *arena, AST *ast, ErrorReporter *err)
{
	s->ast = ast;
	s->err = err;
	s->arena = arena;
	s->depth = 0;
	s->init_node = NO_NODE;
	s->curr_ret = TYPE_VOID;
	s->init_order_count = 0;
	s->init_order_cap = 16;
	s->init_order = arena_alloc(arena, sizeof(uint32_t) * s->init_order_cap);

	symtab_init(&s->table, arena);
}

static void check_reserved_name (Sema *s, char *name, uint16_t len, uint32_t decl_node)
{
	if (s->depth != 0 || len < RESERVED_PREFIX_LEN)
		return;
	if (memcmp(name, RESERVED_PREFIX, RESERVED_PREFIX_LEN) != 0)
		return;

	error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[decl_node]),
			"identifiers starting with '%s' are reserved", RESERVED_PREFIX);
}

static uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		uint8_t data_type, uint32_t decl_node)
{
	check_reserved_name(s, name, len, decl_node);
	uint32_t prev = symtab_find(&s->table, name, len);

	if (prev != NO_SYMBOL && s->table.symbols[prev].depth == s->depth) {
		ASTNode *n = &s->ast->nodes[decl_node];
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' already declared in this scope", (int)len, name);
	}

	Symbol sym = {
		.name = name,
		.len = len,
		.line = s->ast->nodes[decl_node].line,
		.col = s->ast->nodes[decl_node].col,
		.decl = decl_node,
		.depth = s->depth,
		.kind = (uint8_t)kind,
		.type = data_type
	};

	if (kind == SYMBOL_PARAM || (kind == SYMBOL_VAR && (s->depth == 0
					|| s->ast->nodes[decl_node].child != NO_NODE)))
		sym.assigned = 1;

	uint32_t sym_idx = symtab_declare(&s->table, s->arena, sym);
	s->ast->nodes[decl_node].sym = sym_idx;
	return sym_idx;
}

static uint8_t check_expr (Sema *s, uint32_t idx);
static void check_statement (Sema *s, uint32_t idx);
static int stmt_returns (Sema *s, uint32_t idx);
static void check_global_var_init (Sema *s, uint32_t idx);

static uint8_t check_literal (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	switch (n->type)
	{
	case NODE_LIT_i64: n->data_type = TYPE_i64; break;
	case NODE_LIT_u64: n->data_type = TYPE_u64; break;
	case NODE_LIT_CHAR: n->data_type = TYPE_CHAR; break;
	default:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"string literals are not supported yet");
		n->data_type = TYPE_VOID;
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

static void check_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx)
{
	ASTNode *call = &s->ast->nodes[call_idx];
	uint32_t params_node = s->ast->nodes[fn_idx].child;
	uint32_t param = s->ast->nodes[params_node].child;
	uint32_t arg = call->child;

	int count_ok = 1;

	while (arg != NO_NODE) {
		uint8_t arg_type = check_expr(s, arg);
		uint8_t param_type = param != NO_NODE ? s->ast->nodes[param].data_type : TYPE_VOID;
		int types_ok = arg_type == TYPE_ERROR || param_type == TYPE_ERROR
				|| arg_type == param_type;

		if (param == NO_NODE) {
			count_ok = 0;
		} else if (!types_ok) {
			error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[arg]),
					"argument type %s does not match parameter type %s",
					type_name[arg_type], type_name[param_type]);
		}

		if (param != NO_NODE) param = s->ast->nodes[param].next_bro;
		arg = s->ast->nodes[arg].next_bro;
	}

	if (param != NO_NODE) count_ok = 0;

	if (!count_ok)
		error_report(s->err, ERR_ERROR, node_loc(call),
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
		error_report(s->err, ERR_ERROR, node_loc(assign),
				"cannot assign %s to a variable of type %s",
				type_name[r], type_name[l]);
		assign->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	assign->data_type = l;
	return l;
}

static uint8_t check_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t operand_type = check_expr(s, n->child);

	if (operand_type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (operand_type == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator cannot be applied to a value of type %s",
				type_name[operand_type]);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = (n->type == NODE_NOT_L) ? TYPE_i64 : operand_type;
	return n->data_type;
}

static int is_arith (uint8_t node_type)
{
	switch (node_type)
	{
	case NODE_EQ: case NODE_NE: case NODE_GT: case NODE_GE:
	case NODE_LT: case NODE_LE: case NODE_AND_L: case NODE_OR_L:
		return 0;
	default: return 1;
	}
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

static uint8_t check_binary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
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

	if (l != r && is_arith(n->type)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"type mismatch: %s vs %s",
				type_name[l], type_name[r]);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = binop_result_type(n->type, l);
	return n->data_type;
}

static uint8_t check_expr (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

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
	case NODE_EMPTY: n->data_type = TYPE_VOID; return TYPE_VOID;
	case NODE_ERROR: n->data_type=TYPE_ERROR; return TYPE_ERROR;
	default: return check_binary(s, idx);
	}
}

static void check_block_body (Sema *s, uint32_t idx)
{
	uint32_t stmt = s->ast->nodes[idx].child;
	while (stmt != NO_NODE) {
		check_statement(s, stmt);
		stmt = s->ast->nodes[stmt].next_bro;
	}
}

static void check_block (Sema *s, uint32_t idx)
{
	uint32_t mark = s->table.act_count;
	s->depth++;

	check_block_body(s, idx);

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_var_init_type (Sema *s, ASTNode *n, uint8_t init_type)
{
	if (init_type == TYPE_ERROR || n->data_type == TYPE_ERROR)
		return;
	if (init_type == n->data_type)
		return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot initialize '%.*s' (%s) with a value of type %s",
			(int)n->len, n->str, type_name[n->data_type], type_name[init_type]);
}

static void check_var_dec (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	if (n->child != NO_NODE)
		check_var_init_type(s, n, check_expr(s, n->child));

	sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);
}

static void check_main_signature (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	int bad_params = s->ast->nodes[args].child != NO_NODE;
	int bad_ret = s->ast->nodes[ret].data_type != TYPE_i64;

	if (n->len != 4 || memcmp(n->str, "main", 4) != 0)
		return;
	if (bad_params || bad_ret)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'main' must be declared as 'fn main() : i64'");
}

static void check_fn_dec (Sema *s, uint32_t idx)
{
	check_main_signature(s, idx);
	uint32_t args = s->ast->nodes[idx].child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t body = s->ast->nodes[ret].next_bro;

	uint32_t mark = s->table.act_count;
	s->depth++;
	s->curr_ret = s->ast->nodes[ret].data_type;

	uint32_t param = s->ast->nodes[args].child;
	uint32_t count = 0;
	while (param != NO_NODE) {
		ASTNode *p = &s->ast->nodes[param];
		sema_declare(s, p->str, p->len, SYMBOL_PARAM, p->data_type, param);
		count++;
		param = p->next_bro;
	}
	if (count > MAX_PARAMS)
		error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[idx]),
				"too many parameters (max %d)", MAX_PARAMS);

	check_block_body(s, body);

	if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR
			&& !stmt_returns(s, body))
		error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[idx]),
				"function may reach the end without returning");

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_scoped_statement (Sema *s, uint32_t idx)
{
	uint32_t mark = s->table.act_count;
	s->depth++;

	check_statement(s, idx);

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_body (Sema *s, uint32_t idx)
{
	if (s->ast->nodes[idx].type == NODE_BLOCK) {
		check_statement(s, idx);
		return;
	}

	check_scoped_statement(s, idx);
}

static void check_elif_chain (Sema *s, uint32_t idx)
{
	while (idx != NO_NODE) {
		ASTNode *n = &s->ast->nodes[idx];

		if (n->type == NODE_ELIF) {
			uint32_t cond = n->child;
			uint32_t body = s->ast->nodes[cond].next_bro;
			uint8_t type = check_expr(s, cond);
			if (type == TYPE_VOID)
				error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[cond]),
						"expression with resulting type void is not valid as a condition");
			check_body(s, body);
		} else {
			check_body(s, n->child);
		}

		idx = n->next_bro;
	}
}

static void check_if (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;

	uint8_t type = check_expr(s, cond);
	if (type == TYPE_VOID)
		error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[cond]),
				"expression with resulting type void is not valid as a condition");
	check_body(s, body);
	check_elif_chain(s, s->ast->nodes[body].next_bro);
}

static void check_while (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t cond = n->child;
	uint32_t body = s->ast->nodes[cond].next_bro;

	uint8_t type = check_expr(s, cond);
	if (type == TYPE_VOID)
		error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[cond]),
				"expression with resulting type void is not valid as a condition");
	check_body(s, body);
}

static void check_for (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t init = n->child;
	uint32_t cond = s->ast->nodes[init].next_bro;
	uint32_t updt = s->ast->nodes[cond].next_bro;
	uint32_t body = s->ast->nodes[updt].next_bro;

	uint32_t mark = s->table.act_count;
	s->depth++;

	check_statement(s, init);
	uint8_t cond_type = check_expr(s, cond);
	if (cond_type == TYPE_VOID && s->ast->nodes[cond].type != NODE_EMPTY)
		error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[cond]),
				"expression with resulting type void is not valid as a condition");
	check_expr(s, updt);
	check_body(s, body);

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_return (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	if (n->child == NO_NODE) {
		if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"missing return value of type %s",
					type_name[s->curr_ret]);
		return;
	}

	if (s->curr_ret == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"function returning void cannot return a value");
		check_expr(s, n->child);
		return;
	}

	uint8_t val_type = check_expr(s, n->child);
	if (val_type != TYPE_ERROR && s->curr_ret != TYPE_ERROR && val_type != s->curr_ret)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"returning %s but function returns %s",
				type_name[val_type], type_name[s->curr_ret]);
}

static int block_returns (Sema *s, uint32_t idx)
{
	uint32_t stmt = s->ast->nodes[idx].child;
	while (stmt != NO_NODE) {
		if (stmt_returns(s, stmt)) return 1;
		stmt = s->ast->nodes[stmt].next_bro;
	}
	return 0;
}

static int if_returns (Sema *s, uint32_t idx)
{
	uint32_t then_body = s->ast->nodes[s->ast->nodes[idx].child].next_bro;
	uint32_t branch = s->ast->nodes[then_body].next_bro;
	int has_else = 0;

	if (!stmt_returns(s, then_body)) return 0;

	while (branch != NO_NODE) {
		ASTNode *b = &s->ast->nodes[branch];
		uint32_t body = (b->type == NODE_ELIF)
				? s->ast->nodes[b->child].next_bro : b->child;
		if (!stmt_returns(s, body)) return 0;
		if (b->type == NODE_ELSE) has_else = 1;
		branch = b->next_bro;
	}
	return has_else;
}

static int stmt_returns (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	switch (n->type)
	{
	case NODE_RET: return 1;
	case NODE_BLOCK: return block_returns(s, idx);
	case NODE_IF: return if_returns(s, idx);
	default: return 0;
	}
}

static void check_statement (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	switch (n->type)
	{
	case NODE_VAR_DEC: check_var_dec(s, idx); return;
	case NODE_BLOCK: check_block(s, idx); return;
	case NODE_RET: check_return(s, idx); return;
	case NODE_WHILE: check_while(s, idx); return;
	case NODE_FOR: check_for(s, idx); return;
	case NODE_IF: check_if(s, idx); return;
	case NODE_EMPTY: n->data_type = TYPE_VOID; return;
	case NODE_FN_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"functions can only be declared at the top level");
		return;
	default:
		check_expr(s, idx);
		return;
	}
}

static void record_init_order (Sema *s, uint32_t idx)
{
	if (s->init_order_count >= s->init_order_cap) {
		uint32_t old_cap = s->init_order_cap;
		s->init_order_cap <<= 1;
		s->init_order = arena_realloc(s->arena, s->init_order,
				old_cap * sizeof(uint32_t), s->init_order_cap * sizeof(uint32_t));
	}
	s->init_order[s->init_order_count++] = idx;
}

static void check_global_var_init (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	Symbol *sym = &s->table.symbols[n->sym];
	uint32_t saved = s->init_node;

	if (n->child == NO_NODE) return;
	if (sym->state != INIT_PENDING) return;

	sym->state = INIT_CHECKING;
	s->init_node = idx;
	check_var_init_type(s, n, check_expr(s, n->child));
	s->init_node = saved;
	sym->state = INIT_DONE;
	record_init_order(s, idx);
}

static void check_root (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = &s->ast->nodes[idx];
		if (n->type == NODE_VAR_DEC) check_global_var_init(s, idx);
		else if (n->type == NODE_FN_DEC) check_fn_dec(s, idx);
		else error_report(s->err, ERR_ERROR, node_loc(n),
				"only variable and function declarations are allowed at the top level");
		idx = n->next_bro;
	}
}

static void decl_globals (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = &s->ast->nodes[idx];

		if (n->type == NODE_FN_DEC) {
			uint32_t args = n->child;
			uint32_t ret = s->ast->nodes[args].next_bro;
			uint8_t data_type = s->ast->nodes[ret].data_type;
			sema_declare(s, n->str, n->len, SYMBOL_FN, data_type, idx);
		} else if (n->type == NODE_VAR_DEC) {
			sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);
		}

		idx = n->next_bro;
	}
}

void sema_run (Sema *s)
{
	decl_globals(s);
	check_root(s);
}

static const char *kind_name (uint8_t kind)
{
	switch (kind)
	{
	case SYMBOL_FN: return "fn";
	case SYMBOL_VAR: return "var";
	case SYMBOL_PARAM: return "param";
	default: return "unknown";
	}
}

void dump_symbols (SymbolTable *t)
{
	for (uint32_t i = 0; i < t->count; i++) {
		Symbol *sym = &t->symbols[i];
		printf("%s %.*s [%u:%u] depth=%u type=%s\n",
				kind_name(sym->kind), (int)sym->len, sym->name,
				sym->line, sym->col, sym->depth, type_name[sym->type]);
	}
}
