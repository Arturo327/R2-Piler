#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

static uint32_t reject_spread (Sema *s, uint32_t arg);

int sema_dims_match (TypeId arg, TypeId param)
{
	while (type_kind(param) == TK_ARRAY || type_kind(param) == TK_SLICE) {
		int arr_like = type_kind(arg) == TK_ARRAY || type_kind(arg) == TK_SLICE;

		if (!arr_like) return 0;
		if (type_kind(param) == TK_ARRAY && (type_kind(arg) != TK_ARRAY
				|| types[arg].count != types[param].count))
			return 0;
		arg = types[arg].base;
		param = types[param].base;
	}
	return type_kind(arg) != TK_ARRAY && type_kind(arg) != TK_SLICE;
}

int sema_array_slice_compat (TypeId arg, TypeId param)
{
	if (type_kind(param) != TK_ARRAY && type_kind(param) != TK_SLICE) return 0;
	if (!sema_const_compat(type_flat_element(arg), type_flat_element(param))) return 0;
	return sema_dims_match(arg, param);
}

static uint32_t adapt_call_arg (Sema *s, uint32_t arg, TypeId arg_type, TypeId param_type)
{
	int ok;
	uint32_t new_arg = force_cast(s, arg, arg_type, param_type, &ok);

	if (ok) return new_arg;

	const char *hint;

	if (arg_type == TYPE_VOID) hint = "a void value cannot be used here";
	else if (type_is_mixed(param_type))
		hint = "a parameter with some unsized dimensions requires an array or"
				" slice of the same rank and element type, with the same size"
				" in every sized dimension";
	else if (type_kind(param_type) == TK_ARRAY)
		hint = "an array parameter requires an array of exactly that length and element type";
	else if (type_kind(param_type) == TK_SLICE)
		hint = "a slice parameter requires an array or slice of the same rank and element type";
	else hint = "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(&s->ast->nodes[new_arg]),
			"argument type %s does not match parameter type %s; %s",
			type_display_name(arg_type), types[param_type].name, hint);
	return new_arg;
}

static uint32_t count_params (Sema *s, uint32_t params_node)
{
	uint32_t count = 0;

	for (uint32_t p = s->ast->nodes[params_node].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro)
		count++;
	return count;
}

static uint32_t param_at (Sema *s, uint32_t params_node, uint32_t i)
{
	uint32_t p = s->ast->nodes[params_node].child;

	while (i-- > 0 && p != NO_NODE) p = s->ast->nodes[p].next_bro;
	return p;
}

static uint32_t find_param (Sema *s, uint32_t params_node, char *name, uint16_t len)
{
	uint32_t p = s->ast->nodes[params_node].child;
	uint32_t i = 0;

	for (; p != NO_NODE; p = s->ast->nodes[p].next_bro, i++)
		if (s->ast->nodes[p].len == len
				&& memcmp(s->ast->nodes[p].str, name, len) == 0)
			return i;
	return NO_NODE;
}

static int ref_types_match (TypeId got, TypeId want)
{
	if (got == want) return 1;
	if (type_kind(got) != TK_PTR || type_kind(want) != TK_PTR) return 0;
	TypeId gb = types[got].base;
	TypeId wb = types[want].base;
	return gb == wb || (type_is_const(wb) && gb == type_unconst_of(wb));
}

static uint32_t check_ref_arg (Sema *s, uint32_t arg, uint32_t param)
{
	uint8_t kind = s->ast->nodes[arg].type;
	TypeId want = s->ast->nodes[param].data_type;

	if (want == TYPE_ERROR) return arg;
	if (kind != NODE_ID && kind != NODE_DEREF
			&& kind != NODE_INDEX && kind != NODE_DOT) {
		ASTNode *p = s->ast->nodes + param;
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + arg),
				"argument for reference parameter '%.*s' must be a variable",
				(int)p->len, p->str);
		return arg;
	}

	uint32_t node = sema_wrap_addr(s, arg);
	uint8_t saved = s->ref_arg;
	s->ref_arg = 1;
	TypeId got = check_addr(s, node);
	s->ref_arg = saved;

	if (got == TYPE_ERROR) return node;
	if (!ref_types_match(got, want)) {
		ASTNode *p = s->ast->nodes + param;
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + arg),
				"argument type %s does not match reference parameter '%.*s' (&%s)",
				type_display_name(types[got].base), (int)p->len, p->str,
				type_display_name(types[want].base));
	}
	return node;
}

static uint32_t check_arg_type (Sema *s, uint32_t arg, TypeId param_type)
{
	TypeId arg_type = check_expr(s, arg);
	uint32_t out = arg;

	if (param_type != TYPE_VOID && arg_type != TYPE_ERROR
			&& param_type != TYPE_ERROR && arg_type != param_type
			&& !sema_array_slice_compat(arg_type, param_type))
		out = adapt_call_arg(s, arg, arg_type, param_type);
	if (type_kind(arg_type) == TK_ARRAY || type_kind(arg_type) == TK_SLICE)
		agg_untrack_root(s, out);
	return out;
}

static uint32_t check_arg_value (Sema *s, uint32_t arg, uint32_t param)
{
	if (param != NO_NODE && (s->ast->nodes[param].flags & NODE_FLAG_REF_LOWERED))
		return check_ref_arg(s, arg, param);

	TypeId param_type = param == NO_NODE ? TYPE_VOID : s->ast->nodes[param].data_type;

	return check_arg_type(s, arg, param_type);
}

static uint32_t check_vararg_value (Sema *s, uint32_t arg, TypeId elem)
{
	TypeId arg_type = check_expr(s, arg);
	uint32_t out = arg;

	if (arg_type != TYPE_ERROR && elem != TYPE_ERROR && arg_type != elem
			&& !sema_array_slice_compat(arg_type, elem)) {
		int ok;
		out = force_cast(s, arg, arg_type, elem, &ok);
		if (!ok)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + out),
					"variadic argument type %s does not match the element type %s; use an explicit cast with 'as'",
					type_display_name(arg_type), types[elem].name);
	}
	if (type_kind(arg_type) == TK_ARRAY || type_kind(arg_type) == TK_SLICE)
		agg_untrack_root(s, out);
	return out;
}

typedef struct CallArgs {
	Sema *s;
	uint32_t call_idx;
	uint32_t params_node;
	uint32_t *at;
	uint32_t count;
	uint32_t pos;
	uint32_t pos_opt;
	uint32_t pos_opt_param;
	int named;
	int bad;
} CallArgs;

static uint32_t named_param (CallArgs *c, uint32_t arg)
{
	ASTNode *a = c->s->ast->nodes + arg;
	uint32_t pi = find_param(c->s, c->params_node, a->str, a->len);

	if (pi == NO_NODE) {
		error_report(c->s->err, ERR_ERROR, node_loc(a),
				"unknown parameter '%.*s' in call to '%.*s'",
				(int)a->len, a->str, (int)c->s->ast->nodes[c->call_idx].len,
				c->s->ast->nodes[c->call_idx].str);
		return NO_NODE;
	}
	if (c->at[pi] != NO_NODE) {
		error_report(c->s->err, ERR_ERROR, node_loc(a),
				"argument for parameter '%.*s' is given more than once",
				(int)a->len, a->str);
		return NO_NODE;
	}
	return pi;
}

static uint32_t arg_param (CallArgs *c, uint32_t arg)
{
	ASTNode *a = c->s->ast->nodes + arg;

	if (a->type == NODE_NAMED_ARG) {
		c->named = 1;
		uint32_t pi = named_param(c, arg);
		if (pi == NO_NODE) c->bad |= 1;
		return pi;
	}
	if (c->named) {
		error_report(c->s->err, ERR_ERROR, node_loc(a),
				"positional argument cannot follow a named argument");
		c->bad |= 1;
		return NO_NODE;
	}
	if (c->pos >= c->count) {
		c->bad |= 2;
		return NO_NODE;
	}
	uint32_t pi = c->pos++;
	uint32_t param = param_at(c->s, c->params_node, pi);

	if (c->pos_opt == NO_NODE && c->s->ast->nodes[param].child != NO_NODE) {
		c->pos_opt = arg;
		c->pos_opt_param = param;
	}
	return pi;
}

static void report_mixed_optionals (CallArgs *c)
{
	ASTNode *p = c->s->ast->nodes + c->pos_opt_param;

	error_report(c->s->err, ERR_ERROR, node_loc(c->s->ast->nodes + c->pos_opt),
			"optional argument for parameter '%.*s' must be named;"
			" name all optional arguments or pass them all positionally",
			(int)p->len, p->str);
	c->bad |= 1;
}

static int collect_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx, uint32_t *arg_at, uint32_t count)
{
	CallArgs c = { s, call_idx, s->ast->nodes[fn_idx].child, arg_at, count, 0,
			NO_NODE, NO_NODE, 0, 0 };
	uint32_t arg = s->ast->nodes[call_idx].child;

	while (arg != NO_NODE) {
		uint32_t next = s->ast->nodes[arg].next_bro;
		uint32_t pi = arg_param(&c, arg);
		uint32_t expr = s->ast->nodes[arg].type == NODE_NAMED_ARG
				? s->ast->nodes[arg].child : arg;
		uint32_t param = pi == NO_NODE ? NO_NODE
				: param_at(s, c.params_node, pi);
		if (s->ast->nodes[expr].type == NODE_SPREAD) {
			c.bad = 1;
			expr = reject_spread(s, expr);
		}
		uint32_t val = check_arg_value(s, expr, param);

		if (pi != NO_NODE) arg_at[pi] = val;
		arg = next;
	}
	if (!(c.bad & 1) && c.named && c.pos_opt != NO_NODE)
		report_mixed_optionals(&c);
	return c.bad;
}

static int fill_call_defaults (Sema *s, uint32_t fn_idx, uint32_t *arg_at, uint32_t count)
{
	uint32_t params_node = s->ast->nodes[fn_idx].child;
	int missing = 0;

	for (uint32_t i = 0; i < count; i++) {
		uint32_t p = param_at(s, params_node, i);

		if (arg_at[i] != NO_NODE) continue;
		if (s->ast->nodes[p].child == NO_NODE) { missing = 1; continue; }
		arg_at[i] = sema_wrap_cast(s, s->ast->nodes[p].child,
				s->ast->nodes[p].data_type);
	}
	return missing;
}

static void link_call_args (Sema *s, uint32_t call_idx, uint32_t *arg_at, uint32_t count)
{
	uint32_t prev = NO_NODE;

	for (uint32_t i = 0; i < count; i++) {
		uint32_t a = arg_at[i];

		if (a == NO_NODE) continue;
		s->ast->nodes[a].next_bro = NO_NODE;
		if (prev == NO_NODE) s->ast->nodes[call_idx].child = a;
		else s->ast->nodes[prev].next_bro = a;
		prev = a;
	}
	if (prev == NO_NODE) s->ast->nodes[call_idx].child = NO_NODE;
}

static uint32_t reject_spread (Sema *s, uint32_t arg)
{
	ASTNode *n = s->ast->nodes + arg;
	uint32_t child = n->child;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"spread arguments ('...') are only allowed in a typed variadic call");
	if (child != NO_NODE && s->ast->nodes[child].type == NODE_NAMED_ARG) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + child),
				"a spread argument cannot be named");
		child = s->ast->nodes[child].child;
	}
	return child == NO_NODE ? arg : child;
}

static int collect_variadic_args (Sema *s, uint32_t call_idx, uint32_t params_node,
		uint32_t *arg_at, uint32_t count, uint32_t *extra)
{
	uint32_t arg = s->ast->nodes[call_idx].child;
	uint32_t last = NO_NODE;
	uint32_t pos = 0;
	int bad = 0;

	*extra = NO_NODE;
	while (arg != NO_NODE) {
		uint32_t next = s->ast->nodes[arg].next_bro;
		int named = s->ast->nodes[arg].type == NODE_NAMED_ARG;
		uint32_t expr = named ? s->ast->nodes[arg].child : arg;
		uint32_t param = named || pos >= count
				? NO_NODE : param_at(s, params_node, pos);

		if (named) {
			bad = 1;
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + arg),
					"a variadic call cannot use named arguments");
		}
		if (!named && s->ast->nodes[expr].type == NODE_SPREAD) {
			bad = 1;
			expr = reject_spread(s, expr);
		}
		uint32_t val = check_arg_value(s, expr, param);
		arg = next;
		if (param != NO_NODE) { arg_at[pos++] = val; continue; }
		s->ast->nodes[val].next_bro = NO_NODE;
		if (last == NO_NODE) *extra = val;
		else s->ast->nodes[last].next_bro = val;
		last = val;
	}
	return bad;
}

static void link_variadic_args (Sema *s, uint32_t call_idx, uint32_t *arg_at,
		uint32_t count, uint32_t extra)
{
	link_call_args(s, call_idx, arg_at, count);
	if (extra == NO_NODE) return;

	if (s->ast->nodes[call_idx].child == NO_NODE) {
		s->ast->nodes[call_idx].child = extra;
		return;
	}
	uint32_t prev = s->ast->nodes[call_idx].child;

	while (s->ast->nodes[prev].next_bro != NO_NODE)
		prev = s->ast->nodes[prev].next_bro;
	s->ast->nodes[prev].next_bro = extra;
}

typedef struct TypedVarargs {
	Sema *s;
	uint32_t params_node;
	uint32_t fixed;
	TypeId elem;
	uint32_t *arg_at;
	uint32_t pos;
	uint32_t last;
	uint32_t extra;
	int bad;
} TypedVarargs;

static void push_extra (Sema *s, uint32_t val, uint32_t *last, uint32_t *extra)
{
	s->ast->nodes[val].next_bro = NO_NODE;
	if (*last == NO_NODE) *extra = val;
	else s->ast->nodes[*last].next_bro = val;
	*last = val;
}

static int check_spread_arg (Sema *s, uint32_t arg, uint32_t next,
		uint32_t pos, uint32_t fixed, TypeId slice)
{
	ASTNode *n = s->ast->nodes + arg;
	uint32_t child = n->child;
	TypeId got;
	int bad = 0;

	if (child != NO_NODE && s->ast->nodes[child].type == NODE_NAMED_ARG) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + child),
				"a spread argument cannot be named");
		child = s->ast->nodes[child].child;
		bad = 1;
	}
	if (child == NO_NODE) {
		n->data_type = TYPE_ERROR;
		return 1;
	}
	got = check_expr(s, child);
	n->data_type = got == TYPE_ERROR ? TYPE_ERROR : slice;
	if (pos < fixed) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a spread argument cannot be passed to a fixed parameter");
		bad = 1;
	}
	if (next != NO_NODE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a spread argument must be the last argument");
		bad = 1;
	}
	if (got != TYPE_ERROR && got != slice && !sema_array_slice_compat(got, slice)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"spread argument type %s does not match the variadic type %s",
				type_display_name(got), types[slice].name);
		bad = 1;
	}
	return bad;
}

static void typed_vararg_arg (TypedVarargs *v, uint32_t arg, uint32_t next, TypeId slice)
{
	Sema *s = v->s;
	ASTNode *n = s->ast->nodes + arg;
	int named = n->type == NODE_NAMED_ARG;
	uint32_t expr = named ? n->child : arg;
	uint32_t param = named || v->pos >= v->fixed
			? NO_NODE : param_at(s, v->params_node, v->pos);

	if (n->type == NODE_SPREAD) {
		if (v->last != NO_NODE) {
			v->bad = 1;
			error_report(s->err, ERR_ERROR, node_loc(n),
					"a spread argument cannot be combined with other variadic arguments");
		}
		v->bad |= check_spread_arg(s, arg, next, v->pos, v->fixed, slice);
		if (v->last == NO_NODE) push_extra(s, arg, &v->last, &v->extra);
		return;
	}
	if (named) {
		v->bad = 1;
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a variadic call cannot use named arguments");
	}
	uint32_t val = param != NO_NODE ? check_arg_value(s, expr, param)
			: check_vararg_value(s, expr, v->elem);
	if (param != NO_NODE) { v->arg_at[v->pos++] = val; return; }
	push_extra(s, val, &v->last, &v->extra);
}

static int collect_typed_variadic_args (Sema *s, uint32_t call_idx, uint32_t params_node,
		uint32_t *arg_at, uint32_t count, uint32_t *extra)
{
	uint32_t fixed = count - 1;
	TypeId slice = s->ast->nodes[param_at(s, params_node, fixed)].data_type;
	TypedVarargs v = { s, params_node, fixed, types[slice].base, arg_at,
			0, NO_NODE, NO_NODE, 0 };
	uint32_t arg = s->ast->nodes[call_idx].child;

	while (arg != NO_NODE) {
		uint32_t next = s->ast->nodes[arg].next_bro;
		typed_vararg_arg(&v, arg, next, slice);
		arg = next;
	}
	*extra = v.extra;
	return v.bad;
}

void check_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx)
{
	uint32_t params_node = s->ast->nodes[fn_idx].child;
	uint32_t flags = s->ast->nodes[params_node].flags;
	uint32_t count = count_params(s, params_node);
	uint32_t fixed = (flags & NODE_FLAG_VARARG_TYPED) ? count - 1 : count;
	uint32_t arg_at[count + 1];
	uint32_t extra = NO_NODE;
	int variadic = (flags & NODE_FLAG_VARARG) != 0;
	int typed = (flags & NODE_FLAG_VARARG_TYPED) != 0;

	for (uint32_t i = 0; i <= count; i++) arg_at[i] = NO_NODE;

	int bad;
	if (variadic) bad = collect_variadic_args(s, call_idx, params_node, arg_at, count, &extra);
	else if (typed) bad = collect_typed_variadic_args(s, call_idx, params_node, arg_at, count, &extra);
	else bad = collect_call_args(s, call_idx, fn_idx, arg_at, count);
	if (fill_call_defaults(s, fn_idx, arg_at, fixed)) bad |= 4;
	if (variadic) link_variadic_args(s, call_idx, arg_at, count, extra);
	else if (typed) link_variadic_args(s, call_idx, arg_at, fixed, extra);
	else link_call_args(s, call_idx, arg_at, count);

	if ((bad & 6) && !(bad & 1))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + call_idx),
				"wrong number of arguments in call to '%.*s'",
				(int)s->ast->nodes[call_idx].len, s->ast->nodes[call_idx].str);
}

static int value_is_fn_ptr (Sema *s, uint32_t sym_idx)
{
	Symbol *sym = s->table.symbols + sym_idx;
	TypeId type = sym->type;

	if (sym->kind != SYMBOL_VAR && sym->kind != SYMBOL_PARAM) return 0;
	if (sym->by_ref) type = types[type].base;
	return type_kind(type) == TK_PTR && type_kind(types[type].base) == TK_FN;
}

static uint32_t count_call_args (Sema *s, uint32_t first)
{
	uint32_t count = 0;

	while (first != NO_NODE) {
		count++;
		first = s->ast->nodes[first].next_bro;
	}
	return count;
}

static TypeId indirect_callee (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_expr(s, (uint32_t)n->u64);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) return TYPE_ERROR;
	if (type_kind(type) != TK_PTR || type_kind(types[type].base) != TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"expression of type %s is not callable", type_display_name(type));
		return TYPE_ERROR;
	}
	return type;
}

static uint32_t check_indirect_arg (Sema *s, uint32_t arg, TypeId pt)
{
	ASTNode *a = s->ast->nodes + arg;

	if (a->type == NODE_NAMED_ARG) {
		error_report(s->err, ERR_ERROR, node_loc(a),
				"an indirect call cannot use named arguments");
		check_expr(s, a->child);
		return arg;
	}
	if (a->type == NODE_SPREAD) {
		error_report(s->err, ERR_ERROR, node_loc(a),
				"a spread argument ('...') is not supported in an indirect call");
		check_expr(s, a->child);
		return arg;
	}
	return check_arg_type(s, arg, pt);
}

static void check_indirect_args (Sema *s, uint32_t idx, TypeId sig_type)
{
	uint32_t argc = count_call_args(s, s->ast->nodes[idx].child);
	uint32_t fixed = types[sig_type].param_count;
	uint32_t arg = s->ast->nodes[idx].child;
	uint32_t last = NO_NODE;

	for (uint32_t i = 0; i < argc && arg != NO_NODE; i++) {
		uint32_t next = s->ast->nodes[arg].next_bro;
		TypeId pt = i < fixed ? types[sig_type].params[i] : TYPE_ERROR;
		uint32_t out = check_indirect_arg(s, arg, pt);

		s->ast->nodes[out].next_bro = next;
		if (last == NO_NODE) s->ast->nodes[idx].child = out;
		else s->ast->nodes[last].next_bro = out;
		last = out;
		arg = next;
	}
	if (types[sig_type].fn_variadic ? argc < fixed : argc != fixed)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"wrong number of arguments in indirect call: got %u, expected %u",
				argc, fixed);
}

static TypeId check_indirect_call (Sema *s, uint32_t idx)
{
	TypeId type = indirect_callee(s, idx);
	ASTNode *n = s->ast->nodes + idx;
	TypeId sig_type;

	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	sig_type = types[type].base;
	check_indirect_args(s, idx, sig_type);
	n = s->ast->nodes + idx;
	n->sym = type;
	n->data_type = types[sig_type].ret_count ? types[sig_type].rets[0] : TYPE_VOID;
	return n->data_type;
}

static int reject_bad_callee (Sema *s, uint32_t idx, uint32_t sym_idx)
{
	ASTNode *n = s->ast->nodes + idx;
	const char *msg = NULL;

	if (sym_idx == NO_SYMBOL) msg = "is not a known function";
	else {
		SymKind kind = s->table.symbols[sym_idx].kind;

		if (kind == SYMBOL_ENUM) msg = "is an enum type, not a function";
		else if (kind == SYMBOL_TYPE) msg = "is a type, not a function";
		else if (kind != SYMBOL_FN) msg = "is a variable, not a function";
	}
	if (!msg) return 0;
	error_report(s->err, ERR_ERROR, node_loc(n), "'%.*s' %s",
			(int)n->len, n->str, msg);
	n->data_type = TYPE_ERROR;
	return 1;
}

static uint32_t class_qualified_sym (Sema *s, ASTNode *n)
{
	uint16_t sep = 0;

	for (uint16_t i = 0; i + 1 < n->len; i++)
		if (n->str[i] == ':' && n->str[i + 1] == ':') sep = i;
	if (!sep) return NO_SYMBOL;

	TypeId cls = type_canon(sema_lookup_type(s, n->str, sep, 0));
	uint16_t mlen = (uint16_t)(n->len - sep - 2);

	if (type_kind(cls) != TK_CLASS || !mlen) return NO_SYMBOL;
	uint32_t method = class_method_node(s, cls, n->str + sep + 2, mlen);

	return method == NO_NODE ? NO_SYMBOL : s->ast->nodes[method].sym;
}

static uint32_t lookup_fn_name (Sema *s, ASTNode *n)
{
	uint32_t sym = sema_lookup(s, n->str, n->len);
	int local = sym != NO_SYMBOL && s->table.symbols[sym].depth > 0;

	if (sym == NO_SYMBOL) sym = class_qualified_sym(s, n);
	if (!local && s->curr_class != TYPE_ERROR) {
		uint32_t method = class_method_node(s, s->curr_class, n->str, n->len);

		if (method != NO_NODE) sym = s->ast->nodes[method].sym;
	}
	return sym;
}

TypeId check_fn_call (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->flags & NODE_FLAG_INDIRECT) {
		int handled = 0;
		TypeId method = check_method_call(s, idx, &handled);

		if (handled) return method;
		return check_indirect_call(s, idx);
	}
	uint32_t sym_idx = lookup_fn_name(s, n);

	if (sym_idx != NO_SYMBOL && s->table.symbols[sym_idx].kind == SYMBOL_TYPE) {
		TypeId cls = type_canon(s->table.symbols[sym_idx].type);

		if (type_kind(cls) == TK_CLASS) return check_ctor_call(s, idx, cls);
	}
	if (reject_special_call(s, idx, sym_idx)) return TYPE_ERROR;
	if (sym_idx != NO_SYMBOL && value_is_fn_ptr(s, sym_idx)) {
		uint32_t callee = sema_make_id(s, s->ast->nodes + idx);

		n = s->ast->nodes + idx;
		n->u64 = callee;
		n->flags |= NODE_FLAG_INDIRECT;
		return check_indirect_call(s, idx);
	}
	if (reject_bad_callee(s, idx, sym_idx)) return TYPE_ERROR;
	check_method_visibility(s, sym_idx, idx);
	n->sym = sym_idx;
	Symbol *fn = &s->table.symbols[sym_idx];
	if (sym_idx != s->curr_fn) fn->read = 1;
	check_call_args(s, idx, fn->decl);
	n = s->ast->nodes + idx;
	n->data_type = type_unconst_of(fn->type);
	return n->data_type;
}

