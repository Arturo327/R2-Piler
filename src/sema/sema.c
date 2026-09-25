#include "sema/sema.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RESERVED_PREFIX "__r2_"
#define RESERVED_PREFIX_LEN (sizeof(RESERVED_PREFIX) - 1)
#define MAX_PARAMS 255
#define MAX_EXPR_DEPTH 5000

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
	s->expr_depth = 0;
	s->too_deep = 0;

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

static int implicit_cast_ok (uint8_t from, uint8_t to)
{
	const Type *f, *t;

	if (from == to) return 1;
	if (from == TYPE_ERROR || to == TYPE_ERROR) return 0;
	if (from == TYPE_VOID || to == TYPE_VOID) return 0;

	f = &types[from];
	t = &types[to];

	if (f->sign && !t->sign) return 0;
	return t->size > f->size;
}

static uint32_t sema_new_node (Sema *s, ASTNode node)
{
	uint32_t idx = s->ast->count++;

	if (idx >= s->ast->cap) {
		fprintf(stderr, "internal error: AST capacity exceeded by implicit casts\n");
		exit(1);
	}
	s->ast->nodes[idx] = node;
	return idx;
}

static uint32_t sema_wrap_cast (Sema *s, uint32_t child, uint8_t to_type)
{
	ASTNode *c = &s->ast->nodes[child];
	ASTNode cast = {0};

	cast.type = NODE_CAST;
	cast.data_type = to_type;
	cast.line = c->line;
	cast.col = c->col;
	cast.len = 2;
	cast.child = child;
	cast.next_bro = c->next_bro;
	cast.sym = NO_NODE;

	c->next_bro = NO_NODE;
	return sema_new_node(s, cast);
}

typedef struct LitVal {
	uint64_t mag;
	int neg;
} LitVal;

static int lit_fits (LitVal v, uint8_t type)
{
	const Type *t = &types[type];
	unsigned bits = t->size * 8u;
	uint64_t max;

	if (bits == 0) return 0;
	if (t->sign) max = ((uint64_t)1 << (bits - 1)) - 1;
	else max = bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;

	if (v.neg) return t->sign && v.mag <= max + 1;
	return v.mag <= max;
}

static uint64_t lit_bits (LitVal v, uint8_t type)
{
	const Type *t = &types[type];
	unsigned bits = t->size * 8u;
	uint64_t raw = v.neg ? 0 - v.mag : v.mag;

	if (bits >= 64) return raw;
	raw &= ((uint64_t)1 << bits) - 1;
	if (t->sign && ((raw >> (bits - 1)) & 1))
		raw |= ~(uint64_t)0 << bits;
	return raw;
}

static int lit_node_bits (ASTNode *n, uint64_t *out)
{
	switch (n->type)
	{
	case NODE_LIT_i64:
	case NODE_LIT_u64:
		*out = n->u64;
		return 1;
	case NODE_LIT_CHAR:
		*out = (uint64_t)(int64_t)n->chr;
		return 1;
	default:
		return 0;
	}
}

static uint64_t trunc_sext (uint64_t raw, uint8_t t)
{
	LitVal v = { 0 };

	v.mag = raw;
	return lit_bits(v, t);
}

static void make_fold_lit (ASTNode *n, uint64_t bits, uint8_t t)
{
	n->type = types[t].sign ? NODE_LIT_i64 : NODE_LIT_u64;
	n->u64 = bits;
	n->data_type = t;
	n->child = NO_NODE;
}

static int fold_divmod (uint8_t op, uint64_t a, uint64_t b, uint8_t t, uint64_t *out)
{
	if (b == 0) return 0;
	if (types[t].sign) {
		if ((int64_t)a == INT64_MIN && (int64_t)b == -1) return 0;
		if (op == NODE_DIV) *out = (uint64_t)((int64_t)a / (int64_t)b);
		else *out = (uint64_t)((int64_t)a % (int64_t)b);
		return 1;
	}
	*out = op == NODE_DIV ? a / b : a % b;
	return 1;
}

static int fold_value (uint8_t op, uint64_t a, uint64_t b, uint8_t t, uint64_t *out)
{
	switch (op)
	{
	case NODE_ADD: *out = a + b; return 1;
	case NODE_SUB: *out = a - b; return 1;
	case NODE_MUL: *out = a * b; return 1;
	case NODE_AND_A: *out = a & b; return 1;
	case NODE_OR_A: *out = a | b; return 1;
	case NODE_XOR: *out = a ^ b; return 1;
	case NODE_LS:
		if (b >= 64) return 0;
		*out = a << b;
		return 1;
	case NODE_RS:
		if (b >= 64) return 0;
		*out = types[t].sign ? (uint64_t)((int64_t)a >> b) : a >> b;
		return 1;
	case NODE_DIV: case NODE_MOD:
		return fold_divmod(op, a, b, t, out);
	default:
		return 0;
	}
}

static int fold_compare (uint8_t op, uint64_t a, uint64_t b, uint8_t t, uint64_t *out)
{
	int sign = types[t].sign;
	int res;

	switch (op)
	{
	case NODE_EQ: res = a == b; break;
	case NODE_NE: res = a != b; break;
	case NODE_GT: res = sign ? (int64_t)a > (int64_t)b : a > b; break;
	case NODE_GE: res = sign ? (int64_t)a >= (int64_t)b : a >= b; break;
	case NODE_LT: res = sign ? (int64_t)a < (int64_t)b : a < b; break;
	case NODE_LE: res = sign ? (int64_t)a <= (int64_t)b : a <= b; break;
	default: return 0;
	}
	*out = res ? 1 : 0;
	return 1;
}

static uint8_t neg_result_type (ASTNode *c)
{
	if (c->type == NODE_LIT_u64) return TYPE_u64;
	if (c->type == NODE_LIT_i64 && c->data_type == TYPE_u64) return TYPE_i64;
	return c->data_type;
}

static uint8_t try_fold_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	ASTNode *c;
	uint64_t a, v;
	uint8_t t;

	if (n->child == NO_NODE) return n->data_type;
	c = &s->ast->nodes[n->child];
	if (!lit_node_bits(c, &a)) return n->data_type;

	if (n->type == NODE_NOT_L) {
		make_fold_lit(n, a == 0, TYPE_i64);
		return TYPE_i64;
	}

	t = n->data_type;
	if (n->type == NODE_NEG) {
		t = neg_result_type(c);
		v = 0 - a;
	} else if (n->type == NODE_NOT_A) {
		v = ~a;
	} else return n->data_type;

	make_fold_lit(n, trunc_sext(v, t), t);
	return t;
}

static void try_fold_binary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	ASTNode *l;
	ASTNode *r;
	uint64_t a, b, v;
	uint8_t t;

	if (n->child == NO_NODE) return;
	l = &s->ast->nodes[n->child];
	if (l->next_bro == NO_NODE) return;
	r = &s->ast->nodes[l->next_bro];
	if (!lit_node_bits(l, &a) || !lit_node_bits(r, &b)) return;

	if (n->type == NODE_AND_L || n->type == NODE_OR_L) {
		v = n->type == NODE_AND_L ? (a != 0 && b != 0) : (a != 0 || b != 0);
		make_fold_lit(n, v, TYPE_i64);
		return;
	}

	t = l->data_type;
	if (fold_compare(n->type, a, b, t, &v)) {
		make_fold_lit(n, v, TYPE_i64);
		return;
	}
	if (fold_value(n->type, a, b, t, &v))
		make_fold_lit(n, trunc_sext(v, t), t);
}

static int lit_magnitude (ASTNode *lit, LitVal *v)
{
	v->neg = 0;

	switch (lit->type)
	{
	case NODE_LIT_i64:
		v->mag = lit->u64;
		if (lit->i64 < 0 && lit->data_type != TYPE_u64) {
			v->neg = 1;
			v->mag = 0 - lit->u64;
		}
		return 1;
	case NODE_LIT_u64:
		v->mag = lit->u64;
		return 1;
	case NODE_LIT_CHAR:
		v->neg = lit->chr < 0;
		v->mag = v->neg ? 0 - (uint64_t)(int64_t)lit->chr : (uint64_t)lit->chr;
		return 1;
	default:
		return 0;
	}
}

static int flex_literal (Sema *s, uint32_t idx, LitVal *v)
{
	ASTNode *n = &s->ast->nodes[idx];
	int negated = n->type == NODE_NEG;
	ASTNode *lit = negated ? &s->ast->nodes[n->child] : n;

	if (!lit_magnitude(lit, v)) return 0;
	if (!negated) return 1;
	if (v->neg) return 0;
	v->neg = v->mag != 0;
	return 1;
}

static void err_lit_out_of_range (Sema *s, ASTNode *n, LitVal v, uint8_t to)
{
	const char *hint;

	if (v.neg) hint = "; a negative value cannot fit there;"
		" use an explicit cast with 'as' to reinterpret the bits";
	else if (types[to].sign && v.mag > INT64_MAX)
		hint = "; it is too large for i64"
			" (the 'u' suffix makes it a u64 literal)";
	else hint = "; use an explicit cast with 'as' to reinterpret the bits";

	error_report(s->err, ERR_ERROR, node_loc(n),
			"literal %s%llu does not fit in type %s%s",
			v.neg ? "-" : "", (unsigned long long)v.mag, types[to].name, hint);
}

static int retag_literal (Sema *s, uint32_t idx, uint8_t to)
{
	ASTNode *n = &s->ast->nodes[idx];
	LitVal v;
	uint64_t raw;

	if (!flex_literal(s, idx, &v)) return 0;

	raw = lit_bits(v, to);
	if (!lit_fits(v, to)) {
		err_lit_out_of_range(s, n, v, to);
	} else if (n->type == NODE_LIT_u64 && !types[n->data_type].sign
			&& types[to].sign) {
		error_report(s->err, ERR_WARNING, node_loc(n),
				"unsigned literal used as signed type %s; remove the 'u' suffix",
				types[to].name);
	}

	n->u64 = raw;
	if (n->type != NODE_LIT_u64) n->type = NODE_LIT_i64;
	n->data_type = to;
	n->child = NO_NODE;
	return 1;
}

static const uint8_t flex_op[NODE_COUNT] = {
	[NODE_NEG] = 1, [NODE_NOT_A] = 1,
	[NODE_ADD] = 2, [NODE_SUB] = 2, [NODE_MUL] = 2, [NODE_DIV] = 2, [NODE_MOD] = 2,
	[NODE_AND_A] = 2, [NODE_OR_A] = 2, [NODE_XOR] = 2,
	[NODE_LS] = 3, [NODE_RS] = 3
};

static int is_flex_expr (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	LitVal v;

	if (flex_literal(s, idx, &v)) return 1;
	if (!flex_op[n->type] || !is_flex_expr(s, n->child)) return 0;
	if (flex_op[n->type] != 2) return 1;
	return is_flex_expr(s, s->ast->nodes[n->child].next_bro);
}

static void retag_expr (Sema *s, uint32_t idx, uint8_t to)
{
	ASTNode *n;
	uint32_t child;

	if (idx == NO_NODE) return;

	n = &s->ast->nodes[idx];
	child = n->child;
	if (retag_literal(s, idx, to)) return;

	n->data_type = to;
	retag_expr(s, child, to);
	if (flex_op[n->type] == 2)
		retag_expr(s, s->ast->nodes[child].next_bro, to);
}

static uint32_t force_cast (Sema *s, uint32_t child, uint8_t child_type, uint8_t target, int *ok)
{
	*ok = 1;
	if (child_type == target || child_type == TYPE_ERROR || target == TYPE_ERROR)
		return child;
	if (types[target].size && is_flex_expr(s, child)) {
		retag_expr(s, child, target);
		return child;
	}
	if (!implicit_cast_ok(child_type, target)) {
		*ok = 0;
		return child;
	}
	return sema_wrap_cast(s, child, target);
}

static void reserve_cast_nodes (Sema *s)
{
	uint32_t old_cap = s->ast->cap;
	uint32_t new_cap = s->ast->count * 2 + 64;

	if (new_cap <= old_cap) return;

	s->ast->nodes = arena_realloc(s->arena, s->ast->nodes,
			(size_t)old_cap * sizeof(ASTNode), (size_t)new_cap * sizeof(ASTNode));
	s->ast->cap = new_cap;
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
	case NODE_LIT_i64: n->data_type = n->u64 > INT64_MAX ? TYPE_u64 : TYPE_i64; break;
	case NODE_LIT_u64: n->data_type = TYPE_u64; break;
	case NODE_LIT_CHAR: n->data_type = TYPE_CHAR; break;
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
		int ok = 1;

		if (param == NO_NODE) {
			count_ok = 0;
		} else if (arg_type != TYPE_ERROR && param_type != TYPE_ERROR
				&& arg_type != param_type) {
			arg = force_cast(s, arg, arg_type, param_type, &ok);
			if (prev == NO_NODE) call->child = arg;
			else s->ast->nodes[prev].next_bro = arg;
			if (!ok) error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[arg]),
					"argument type %s does not match parameter type %s; use an explicit cast with 'as'",
					types[arg_type].name, types[param_type].name);
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
			error_report(s->err, ERR_ERROR, node_loc(assign),
					"cannot assign %s to a variable of type %s; use an explicit cast with 'as'",
					types[r].name, types[l].name);
			assign->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}

	assign->data_type = l;
	return l;
}

static uint8_t check_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t op = n->type;
	uint8_t operand_type = check_expr(s, n->child);
	uint8_t result_type;

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
	result_type = try_fold_unary(s, idx);

	if (op == NODE_NEG && !types[result_type].sign)
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
	ASTNode *n = &s->ast->nodes[idx];
	int tru;

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

static const uint8_t cmp_op[NODE_COUNT] = {
	[NODE_EQ] = 1, [NODE_NE] = 1, [NODE_GT] = 1,
	[NODE_GE] = 1, [NODE_LT] = 1, [NODE_LE] = 1
};

static uint8_t common_type (Sema *s, ASTNode *n, uint32_t left, uint32_t right,
		uint8_t l, uint8_t r)
{
	int l_flex = is_flex_expr(s, left);
	int r_flex = is_flex_expr(s, right);
	uint8_t other = l_flex ? r : l;
	LitVal v;

	if (l_flex != r_flex) {
		int fits = !flex_literal(s, l_flex ? left : right, &v) || lit_fits(v, other);

		if (fits || !cmp_op[n->type] || !implicit_cast_ok(other, l_flex ? l : r))
			return other;
	}
	if (implicit_cast_ok(l, r)) return r;
	if (implicit_cast_ok(r, l)) return l;
	if (l_flex && r_flex) return types[l].sign ? r : l;
	return TYPE_ERROR;
}

static void convert_operand (Sema *s, uint32_t *idx, uint8_t *type, uint8_t to)
{
	if (is_flex_expr(s, *idx)) {
		retag_expr(s, *idx, to);
	} else {
		*idx = sema_wrap_cast(s, *idx, to);
	}
	*type = to;
}

static int unify_operands (Sema *s, ASTNode *n, uint32_t *left, uint32_t *right,
		uint8_t *l, uint8_t *r)
{
	uint8_t to = common_type(s, n, *left, *right, *l, *r);

	if (to == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"type mismatch: %s vs %s (use 'as' to convert explicitly)",
				types[*l].name, types[*r].name);
		return 0;
	}
	if (*l != to) convert_operand(s, left, l, to);
	if (*r != to) convert_operand(s, right, r, to);
	return 1;
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

	if (n->type == NODE_AND_L || n->type == NODE_OR_L) {
		const char *op = n->type == NODE_AND_L ? "&&" : "||";
		warn_const_operand(s, left, op);
		warn_const_operand(s, right, op);
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
	return n->data_type;
}

static uint8_t check_expr_node (Sema *s, uint32_t idx)
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
	case NODE_CAST: return check_cast(s, idx);
	case NODE_EMPTY: n->data_type = TYPE_VOID; return TYPE_VOID;
	case NODE_ERROR: n->data_type=TYPE_ERROR; return TYPE_ERROR;
	default: return check_binary(s, idx);
	}
}

static uint8_t check_expr (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
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
	int ok;

	if (init_type == TYPE_ERROR || n->data_type == TYPE_ERROR)
		return;
	if (init_type == n->data_type)
		return;

	n->child = force_cast(s, n->child, init_type, n->data_type, &ok);
	if (ok) return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"can't initialize '%.*s' (%s) with a value of type %s; use an explicit cast with 'as'",
			(int)n->len, n->str, types[n->data_type].name, types[init_type].name);
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

	check_body(s, body);
	check_expr(s, updt);

	s->depth--;
	symtab_pop_scope(&s->table, mark);
}

static void check_return (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t val_type;
	int ok;

	if (n->child == NO_NODE) {
		if (s->curr_ret != TYPE_VOID && s->curr_ret != TYPE_ERROR)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"missing return value of type %s",
					types[s->curr_ret].name);
		return;
	}

	if (s->curr_ret == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"function returning void cannot return a value");
		check_expr(s, n->child);
		return;
	}

	val_type = check_expr(s, n->child);
	if (val_type == TYPE_ERROR || s->curr_ret == TYPE_ERROR || val_type == s->curr_ret)
		return;

	n->child = force_cast(s, n->child, val_type, s->curr_ret, &ok);
	if (ok) return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot return %s from a function returning %s; use an explicit cast with 'as'",
			types[val_type].name, types[s->curr_ret].name);
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
			if (n->len == 4 && memcmp(n->str, "main", 4) == 0)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"'main' must be a function");
			sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);
		}

		idx = n->next_bro;
	}
}

void sema_run (Sema *s)
{
	reserve_cast_nodes(s);
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
				sym->line, sym->col, sym->depth, types[sym->type].name);
	}
}
