#include "sema/common.h"

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

static int is_untyped (uint8_t type)
{
	return type == TYPE_UNTYPED_INT || type == TYPE_UNTYPED_UINT
			|| type == TYPE_UNTYPED_CHAR;
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

static uint64_t trunc_sext (uint64_t raw, uint8_t t)
{
	LitVal v = { 0 };

	v.mag = raw;
	return lit_bits(v, t);
}

static int lit_node_bits (Sema *s, ASTNode *n, uint64_t *out)
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
	case NODE_CAST:
		if (!lit_node_bits(s, &s->ast->nodes[n->child], out))
			return 0;
		*out = trunc_sext(*out, n->data_type);
		return 1;
	default:
		return 0;
	}
}

int is_literal_node (Sema *s, uint32_t idx)
{
	uint64_t v;
	return lit_node_bits(s, s->ast->nodes + idx, &v);
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
	if (c->type == NODE_LIT_u64) return TYPE_UNTYPED_UINT;
	if (c->type == NODE_LIT_i64 && c->data_type == TYPE_UNTYPED_UINT) return TYPE_UNTYPED_INT;
	return c->data_type;
}

uint8_t try_fold_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	ASTNode *c;
	uint64_t a, v;
	uint8_t t;

	if (n->child == NO_NODE) return n->data_type;
	c = s->ast->nodes + n->child;
	if (!lit_node_bits(s, c, &a)) return n->data_type;
	if (c->data_type == TYPE_ERROR) return n->data_type;

	if (n->type == NODE_NOT_L) {
		make_fold_lit(n, a == 0, TYPE_i64);
		return TYPE_i64;
	}

	t = n->data_type;
	if (n->type == NODE_NEG) {
		t = neg_result_type(c);
		v = 0 - a;
		n->len = (uint16_t)(n->len + c->len);
	} else if (n->type == NODE_NOT_A) {
		v = ~a;
	} else return n->data_type;

	make_fold_lit(n, trunc_sext(v, t), t);
	return t;
}

void try_fold_binary (Sema *s, uint32_t idx)
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

	if (!lit_node_bits(s, l, &a) || !lit_node_bits(s, r, &b)) return;
	if (l->data_type == TYPE_ERROR || r->data_type == TYPE_ERROR) return;

	if (n->type == NODE_AND_L || n->type == NODE_OR_L) {
		v = n->type == NODE_AND_L ? (a != 0 && b != 0) : (a != 0 || b != 0);
		make_fold_lit(n, v, TYPE_i64);
		return;
	}

	t = l->data_type;

	if ((n->type == NODE_DIV || n->type == NODE_MOD) && b == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"division by zero in constant expression");
		n->data_type = TYPE_ERROR;
		return;
	}

	if (fold_compare(n->type, a, b, t, &v)) {
		make_fold_lit(n, v, TYPE_i64);
		return;
	}

	if (fold_value(n->type, a, b, t, &v)) {
		uint64_t bits = is_untyped(t) ? v : trunc_sext(v, t);

		make_fold_lit(n, bits, t);
	}
}

static int lit_magnitude (ASTNode *lit, LitVal *v)
{
	v->neg = 0;

	switch (lit->type)
	{
	case NODE_LIT_i64:
		v->mag = lit->u64;
		if (lit->i64 < 0 && lit->data_type != TYPE_UNTYPED_UINT) {
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

static const char *type_display_name (uint8_t type)
{
	switch (type)
	{
	case TYPE_UNTYPED_INT: return types[TYPE_i64].name;
	case TYPE_UNTYPED_UINT: return types[TYPE_u64].name;
	case TYPE_UNTYPED_CHAR: return types[TYPE_i8].name;
	default: return types[type].name;
	}
}

static void err_lit_out_of_range (Sema *s, ASTNode *n, LitVal v, uint8_t to)
{
	const char *name = type_display_name(to);
	if (v.neg && !types[to].sign)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"literal -%llu does not fit in type %s;"
				" %s is unsigned and cannot hold a negative value;"
				" use an explicit cast with 'as' to reinterpret the bits",
				(unsigned long long)v.mag, name, name);
	else if (v.neg) error_report(s->err, ERR_ERROR, node_loc(n),
			"literal -%llu does not fit in type %s;"
			" use an explicit cast with 'as' to reinterpret the bits",
			(unsigned long long)v.mag, name);
	else if (types[to].sign && v.mag > INT64_MAX)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"literal %llu does not fit in type %s;"
				" it is too large (the 'u' suffix makes it a u64 literal)",
				(unsigned long long)v.mag, name);
	else error_report(s->err, ERR_ERROR, node_loc(n),
			"literal %llu does not fit in type %s;"
			" use an explicit cast with 'as' to reinterpret the bits",
			(unsigned long long)v.mag, name);
}

static int retag_literal (Sema *s, uint32_t idx, uint8_t to)
{
	ASTNode *n = &s->ast->nodes[idx];
	LitVal v;
	uint64_t raw;

	if (n->data_type == TYPE_ERROR) return 1;
	if (!flex_literal(s, idx, &v)) return 0;

	raw = lit_bits(v, to);
	if (!lit_fits(v, to)) {
		err_lit_out_of_range(s, n, v, to);
		n->u64 = raw;
		if (n->type != NODE_LIT_u64) n->type = NODE_LIT_i64;
		n->data_type = TYPE_ERROR;
		n->child = NO_NODE;
		return 1;
	}

	if (n->type == NODE_LIT_u64 && !types[n->data_type].sign
			&& types[to].sign) {
		error_report(s->err, ERR_WARNING, node_loc(n),
				"unsigned literal used as signed type %s; remove the 'u' suffix",
				type_display_name(to));
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

static uint8_t default_type (uint8_t type)
{
	switch (type)
	{
	case TYPE_UNTYPED_INT: return TYPE_i64;
	case TYPE_UNTYPED_UINT: return TYPE_u64;
	case TYPE_UNTYPED_CHAR: return TYPE_i8;
	default: return type;
	}
}

uint8_t finalize_type (Sema *s, uint32_t idx, uint8_t type)
{
	uint8_t to = default_type(type);

	if (to != type) retag_expr(s, idx, to);
	return to;
}

uint32_t force_cast (Sema *s, uint32_t child, uint8_t child_type, uint8_t target, int *ok)
{
	*ok = 1;
	if (child_type == target || child_type == TYPE_ERROR || target == TYPE_ERROR)
		return child;
	if (types[target].size && is_untyped(child_type)) {
		retag_expr(s, child, target);
		return child;
	}
	if (!implicit_cast_ok(child_type, target)) {
		*ok = 0;
		return child;
	}
	return sema_wrap_cast(s, child, target);
}

static uint8_t common_type (uint8_t l, uint8_t r)
{
	int l_flex = is_untyped(l);
	int r_flex = is_untyped(r);

	if (l_flex != r_flex) return l_flex ? r : l;
	if (implicit_cast_ok(l, r)) return r;
	if (implicit_cast_ok(r, l)) return l;
	if (l_flex && r_flex) return types[l].sign ? r : l;
	return TYPE_ERROR;
}

static void convert_operand (Sema *s, uint32_t *idx, uint8_t *type, uint8_t to)
{
	if (is_untyped(*type)) {
		retag_expr(s, *idx, to);
	} else {
		*idx = sema_wrap_cast(s, *idx, to);
	}
	*type = to;
}

int unify_operands (Sema *s, ASTNode *n, uint32_t *left, uint32_t *right,
		uint8_t *l, uint8_t *r)
{
	uint8_t to = common_type(*l, *r);

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
