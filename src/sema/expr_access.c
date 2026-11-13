#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

TypeId check_index (Sema *s, uint32_t idx);
static TypeId check_len_field (Sema *s, uint32_t idx, uint32_t left, TypeId type);
static TypeId check_dot_value (Sema *s, uint32_t idx);
static TypeId check_field (Sema *s, uint32_t idx, TypeId st);

static TypeId check_member_base (Sema *s, uint32_t child)
{
	ASTNode *n = s->ast->nodes + child;

	if (n->type != NODE_ID) return check_expr(s, child);

	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);
	if (sym_idx == NO_SYMBOL || !s->table.symbols[sym_idx].agg_slots)
		return check_expr(s, child);

	TypeId type = check_var_ref(s, child, 1);
	n = s->ast->nodes + child;
	if (type != TYPE_ERROR && n->sym != NO_SYMBOL && child != s->write_root) {
		Symbol *sym = &s->table.symbols[n->sym];

		sym->read = 1;
		sym->pending_write = 0;
	}
	return type;
}

typedef struct {
	TypeId type;
	uint64_t size;
	uint64_t align;
} OperandInfo;

static void info_from_type (OperandInfo *info, TypeId type)
{
	info->type = type;
	info->size = types[type].size;
	info->align = type_align(type_repr(type));
}

static int operand_named_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);

	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is not declared", (int)n->len, n->str);
		return 0;
	}

	Symbol *sym = s->table.symbols + sym_idx;
	if (sym->kind == SYMBOL_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of %s cannot be a function", op);
		return 0;
	}
	if (sym->kind != SYMBOL_VAR && sym->kind != SYMBOL_PARAM
			&& sym->kind != SYMBOL_TYPE && sym->kind != SYMBOL_ENUM)
		return 0;

	info->type = sym->type;
	return 1;
}

static int operand_field_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op);
static int operand_ptr_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op);

static int operand_expr_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	switch (n->type)
	{
	case NODE_ID:
		if (!operand_named_info(s, n, info, op)) return 0;
		if (info->type == TYPE_ERROR) return 1;
		info_from_type(info, info->type);
		return 1;
	case NODE_LIT_STR:
		info_from_type(info, TYPE_STR);
		info->size = (uint64_t)n->len + 1;
		info->align = 1;
		return 1;
	case NODE_LIT_NULL: info_from_type(info, TYPE_NULL); return 1;
	case NODE_LIT_i64: info_from_type(info, TYPE_i64); return 1;
	case NODE_LIT_u64: info_from_type(info, TYPE_u64); return 1;
	case NODE_LIT_CHAR: info_from_type(info, TYPE_i8); return 1;
	case NODE_DEREF:
	case NODE_INDEX:
		return operand_ptr_info(s, n, info, op);
	case NODE_DOT:
		return operand_field_info(s, n, info, op);
	default:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of %s must be a variable, dereference, index, string literal or type", op);
		return 0;
	}
}

static int operand_ptr_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	const char *what = n->type == NODE_INDEX ? "index" : "dereference";
	OperandInfo base;

	if (!operand_expr_info(s, s->ast->nodes + n->child, &base, op)) return 0;
	if (base.type == TYPE_ERROR) {
		info->type = TYPE_ERROR;
		return 1;
	}
	TypeKind kind = type_kind(base.type);
	if (kind == TK_ARRAY || kind == TK_SLICE) {
		info_from_type(info, types[base.type].base);
		return 1;
	}
	if (kind != TK_PTR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot %s a value of type %s", what, type_display_name(base.type));
		return 0;
	}
	if (types[base.type].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot %s 'ptr void'; cast it to a typed pointer first", what);
		return 0;
	}
	info_from_type(info, types[base.type].base);
	return 1;
}

static int operand_field_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	OperandInfo left;

	if (!operand_expr_info(s, s->ast->nodes + n->child, &left, op)) return 0;
	if (left.type == TYPE_ERROR) {
		info->type = TYPE_ERROR;
		return 1;
	}
	TypeId st = left.type;
	if (type_kind(st) == TK_PTR) st = types[st].base;
	if (type_kind(st) != TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of %s must be a variable, dereference, index, string literal or type", op);
		return 0;
	}
	const StructField *f = find_field(st, n->str, n->len);
	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(n), "struct '%s' has no field '%.*s'",
				types[st].name, (int)n->len, n->str);
		return 0;
	}
	info_from_type(info, f->type);
	return 1;
}

static int operand_info (Sema *s, uint32_t idx, OperandInfo *info, const char *op)
{
	ASTNode *n = s->ast->nodes + idx;

	info->type = TYPE_ERROR;
	if (n->child == NO_NODE) {
		TypeId type = resolve_type_node(s, n);

		n = s->ast->nodes + idx;
		if (type == TYPE_ERROR) return 0;
		if (type == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"%s cannot be applied to type void", op);
			return 0;
		}
		info_from_type(info, type);
		return 1;
	}
	if (!operand_expr_info(s, s->ast->nodes + n->child, info, op)) return 0;
	if (info->type == TYPE_ERROR) return 0;
	if (info->type == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"%s cannot be applied to a value of type void", op);
		return 0;
	}
	return 1;
}

static TypeId fold_layout_lit (Sema *s, uint32_t idx, uint64_t value)
{
	ASTNode *n = s->ast->nodes + idx;

	n->type = NODE_LIT_i64;
	n->i64 = (int64_t)value;
	n->data_type = TYPE_UNTYPED_INT;
	n->child = NO_NODE;
	n->type_node = NO_TYPE_NODE;
	n->sym = NO_SYMBOL;
	return TYPE_UNTYPED_INT;
}

static TypeId err_incomplete_type (Sema *s, uint32_t idx, TypeId type)
{
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"type '%s' is incomplete", types[type].name);
	s->ast->nodes[idx].data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

TypeId check_sizeof (Sema *s, uint32_t idx)
{
	OperandInfo info;

	if (!operand_info(s, idx, &info, "sizeof")) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return fold_layout_lit(s, idx, info.size);
}

TypeId check_alignof (Sema *s, uint32_t idx)
{
	OperandInfo info;

	if (!operand_info(s, idx, &info, "alignof")) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId repr = type_repr(info.type);
	if (type_kind(repr) == TK_STRUCT && types[repr].layout != 2)
		return err_incomplete_type(s, idx, repr);
	return fold_layout_lit(s, idx, info.align);
}

static int offsetof_step (Sema *s, ASTNode *field, TypeId *type, uint64_t *off, int last)
{
	const StructField *f = find_field(*type, field->str, field->len);

	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(field),
				"struct '%s' has no field '%.*s'", types[*type].name,
				(int)field->len, field->str);
		return 0;
	}
	if (f->bits) {
		error_report(s->err, ERR_ERROR, node_loc(field),
				"offsetof cannot be applied to bitfield '%.*s'",
				(int)field->len, field->str);
		return 0;
	}
	if (!last && type_kind(f->type) != TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(field),
				"offsetof cannot traverse field '%.*s' of type %s",
				(int)field->len, field->str, types[f->type].name);
		return 0;
	}

	*off += f->offset;
	*type = f->type;
	return 1;
}

TypeId check_offsetof (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = resolve_type_node(s, n);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) != TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"offsetof requires a struct or union type, not '%s'",
				types[type].name);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (types[type].layout != 2) return err_incomplete_type(s, idx, type);

	uint64_t off = 0;
	for (uint32_t seg = n->child; seg != NO_NODE; seg = s->ast->nodes[seg].next_bro) {
		int last = s->ast->nodes[seg].next_bro == NO_NODE;

		if (!offsetof_step(s, s->ast->nodes + seg, &type, &off, last)) {
			n->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}

	return fold_layout_lit(s, idx, off);
}

TypeId check_deref (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_expr(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) != TK_PTR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot dereference a value of type %s", type_display_name(type));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (types[type].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot dereference 'ptr void'; cast it to a typed pointer first");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = types[type].base;
	warn_null_deref(s, idx, n->child);
	return n->data_type;
}

TypeId check_addr (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *child = s->ast->nodes + n->child;
	TypeId type;
	uint32_t saved_addr = s->addr_operand;

	s->addr_operand = 1;
	if (child->type == NODE_ID) {
		type = check_var_ref(s, n->child, 0);
		n = s->ast->nodes + idx;
		child = s->ast->nodes + n->child;
		if (type != TYPE_ERROR && child->sym != NO_SYMBOL)
			s->table.symbols[child->sym].addressed = 1;
		mark_read(s, n->child);
	} else if (child->type == NODE_DEREF) {
		type = check_deref(s, n->child);
		n = s->ast->nodes + idx;
	} else if (child->type == NODE_INDEX) {
		type = check_index(s, n->child);
		n = s->ast->nodes + idx;
	} else if (child->type == NODE_DOT) {
		type = check_dot(s, n->child);
		n = s->ast->nodes + idx;
		if (type != TYPE_ERROR && (s->ast->nodes[n->child].flags & NODE_FLAG_BITFIELD)) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot take the address of bitfield '%.*s'",
					(int)s->ast->nodes[n->child].len, s->ast->nodes[n->child].str);
			type = TYPE_ERROR;
		} else if (type != TYPE_ERROR && s->ast->nodes[n->child].type != NODE_DOT) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"operand of '$' must be a variable, a dereference or an index");
			type = TYPE_ERROR;
		}
	} else {
		s->addr_operand = saved_addr;
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of '$' must be a variable, a dereference or an index");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	s->addr_operand = saved_addr;

	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot take the address of an array; the array itself decays to a pointer to its first element, use it directly");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (type == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot take the address of a value of type void");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = type_ptr_to(type);
	if (n->data_type == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return TYPE_ERROR;
	}
	agg_untrack_root(s, n->child);
	return n->data_type;
}

int sema_array_slice_compat (TypeId arg, TypeId param)
{
	if (type_kind(param) != TK_ARRAY && type_kind(param) != TK_SLICE) return 0;
	if (type_flat_element(arg) != type_flat_element(param)) return 0;

	while (type_kind(param) == TK_ARRAY || type_kind(param) == TK_SLICE) {
		if (type_kind(arg) != TK_ARRAY && type_kind(arg) != TK_SLICE) return 0;
		if (type_kind(param) == TK_ARRAY
				&& (type_kind(arg) != TK_ARRAY
				|| types[arg].count != types[param].count))
			return 0;
		arg = types[arg].base;
		param = types[param].base;
	}
	return type_kind(arg) != TK_ARRAY && type_kind(arg) != TK_SLICE;
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

static uint32_t check_arg_value (Sema *s, uint32_t arg, TypeId param_type)
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

typedef struct CallArgs {
	Sema *s;
	uint32_t call_idx;
	uint32_t params_node;
	uint32_t *at;
	uint32_t count;
	uint32_t pos;
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
	if (c->pos < c->count) return c->pos++;
	c->bad |= 2;
	return NO_NODE;
}

static TypeId param_type (Sema *s, uint32_t params_node, uint32_t pi)
{
	if (pi == NO_NODE) return TYPE_VOID;
	return s->ast->nodes[param_at(s, params_node, pi)].data_type;
}

static int collect_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx, uint32_t *arg_at, uint32_t count)
{
	CallArgs c = { s, call_idx, s->ast->nodes[fn_idx].child, arg_at, count, 0, 0, 0 };
	uint32_t arg = s->ast->nodes[call_idx].child;

	while (arg != NO_NODE) {
		uint32_t next = s->ast->nodes[arg].next_bro;
		uint32_t pi = arg_param(&c, arg);
		uint32_t expr = s->ast->nodes[arg].type == NODE_NAMED_ARG
				? s->ast->nodes[arg].child : arg;
		uint32_t val = check_arg_value(s, expr, param_type(s, c.params_node, pi));

		if (pi != NO_NODE) arg_at[pi] = val;
		arg = next;
	}
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

static void check_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx)
{
	uint32_t params_node = s->ast->nodes[fn_idx].child;
	uint32_t count = count_params(s, params_node);
	uint32_t arg_at[count + 1];

	for (uint32_t i = 0; i <= count; i++) arg_at[i] = NO_NODE;
	int bad = collect_call_args(s, call_idx, fn_idx, arg_at, count);
	if (fill_call_defaults(s, fn_idx, arg_at, count)) bad |= 4;
	link_call_args(s, call_idx, arg_at, count);

	if ((bad & 6) && !(bad & 1))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + call_idx),
				"wrong number of arguments in call to '%.*s'",
				(int)s->ast->nodes[call_idx].len, s->ast->nodes[call_idx].str);
}

TypeId check_fn_call (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);

	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'%.*s' is not a known function",
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (s->table.symbols[sym_idx].kind == SYMBOL_ENUM) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is an enum type, not a function", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (s->table.symbols[sym_idx].kind == SYMBOL_TYPE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is a type, not a function", (int)n->len, n->str);
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
	if (sym_idx != s->curr_fn) fn->read = 1;
	check_call_args(s, idx, fn->decl);
	n->data_type = fn->type;
	return n->data_type;
}

static TypeId check_len_field (Sema *s, uint32_t idx, uint32_t left, TypeId type)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeKind kind = type_kind(type);

	if (n->len != 3 || memcmp(n->str, "len", 3) != 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"type %s has no field '%.*s'", types[type].name,
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	mark_read(s, left);
	if (kind == TK_SLICE) {
		n->data_type = TYPE_UNTYPED_INT;
		return TYPE_UNTYPED_INT;
	}

	uint32_t count = types[type].count;
	n->type = NODE_LIT_i64;
	n->i64 = (int64_t)count;
	n->data_type = TYPE_UNTYPED_INT;
	n->child = NO_NODE;
	n->sym = NO_SYMBOL;
	n->type_node = NO_TYPE_NODE;
	return TYPE_UNTYPED_INT;
}

static TypeId check_dot_index (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_index(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) == TK_STRUCT) return check_field(s, idx, type);
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE)
		return check_len_field(s, idx, n->child, type);

	error_report(s->err, ERR_ERROR, node_loc(n),
			"a value of type %s is not an array or a slice", type_display_name(type));
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

static TypeId check_dot_var (Sema *s, uint32_t idx, Symbol *sym)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *left = s->ast->nodes + n->child;
	TypeKind kind = type_kind(sym->type);

	if (kind != TK_ARRAY && kind != TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(left),
				"'%.*s' is not an array or a slice", (int)left->len, left->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId type = check_var_ref(s, n->child, 1);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return check_len_field(s, idx, n->child, type);
}

static TypeId check_dot_enum (Sema *s, uint32_t idx, Symbol *sym)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *left = s->ast->nodes + n->child;
	TypeId val_type = sym->kind == SYMBOL_TYPE ? type_canon(sym->type) : sym->type;
	TypeId enum_id = type_repr(val_type);

	if (sym->kind != SYMBOL_ENUM && (sym->kind != SYMBOL_TYPE
			|| type_kind(enum_id) != TK_ENUM)) {
		error_report(s->err, ERR_ERROR, node_loc(left),
				"'%.*s' is not an enum type", (int)left->len, left->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (enum_id == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	const EnumVariant *v = find_variant(enum_id, n->str, n->len);
	if (!v) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enum '%s' has no variant '%.*s'",
				types[enum_id].name, (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->type = NODE_LIT_u64;
	n->u64 = v->value;
	n->data_type = val_type;
	n->child = NO_NODE;
	n->sym = NO_SYMBOL;
	n->type_node = NO_TYPE_NODE;
	return n->data_type;
}

static TypeId check_field (Sema *s, uint32_t idx, TypeId st)
{
	ASTNode *n = s->ast->nodes + idx;
	const StructField *f = find_field(st, n->str, n->len);

	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"struct '%s' has no field '%.*s'", types[st].name,
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (f->bits) n->flags |= NODE_FLAG_BITFIELD;
	n->data_type = f->type;
	agg_member_read(s, idx);
	return f->type;
}

static TypeId check_dot_value (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_member_base(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) == TK_PTR && type_kind(types[type].base) == TK_STRUCT) {
		uint32_t deref;

		warn_null_deref(s, idx, n->child);
		type = types[type].base;
		deref = sema_wrap_deref(s, n->child, type);
		s->ast->nodes[idx].child = deref;
		n = s->ast->nodes + idx;
	}
	if (type_kind(type) == TK_STRUCT) return check_field(s, idx, type);
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE)
		return check_len_field(s, idx, n->child, type);
	if (n->len == 3 && memcmp(n->str, "len", 3) == 0)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a value of type %s is not an array or a slice",
				type_display_name(type));
	else error_report(s->err, ERR_ERROR, node_loc(n),
			"a value of type %s is not an enum type",
			type_display_name(type));
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

TypeId check_dot (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *left = s->ast->nodes + n->child;

	if (left->type == NODE_INDEX)
		return check_dot_index(s, idx);

	if (left->type != NODE_ID) return check_dot_value(s, idx);

	uint32_t sym_idx = symtab_find(&s->table, left->str, left->len);
	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(left),
				"'%.*s' is not declared", (int)left->len, left->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	Symbol *sym = s->table.symbols + sym_idx;
	TypeKind kind = type_kind(sym->type);

	if (kind == TK_STRUCT || (kind == TK_PTR
			&& type_kind(types[sym->type].base) == TK_STRUCT))
		return check_dot_value(s, idx);
	if (sym->kind == SYMBOL_VAR || sym->kind == SYMBOL_PARAM)
		return check_dot_var(s, idx, sym);
	if (sym->kind == SYMBOL_ENUM || (sym->kind == SYMBOL_TYPE
			&& type_kind(type_repr(sym->type)) == TK_ENUM))
		return check_dot_enum(s, idx, sym);
	error_report(s->err, ERR_ERROR, node_loc(left),
			"'%.*s' is not an enum type", (int)left->len, left->str);
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

TypeId check_index (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t base = n->child;
	uint32_t index = s->ast->nodes[base].next_bro;
	TypeId ptr = check_member_base(s, base);
	TypeId off = check_expr(s, index);

	n = s->ast->nodes + idx;
	if (ptr == TYPE_ERROR || off == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	TypeKind kind = type_kind(ptr);
	if (kind != TK_PTR && kind != TK_ARRAY && kind != TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index a value of type %s", type_display_name(ptr));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (kind == TK_PTR && types[ptr].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index 'ptr void'; cast it to a typed pointer first");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (!is_integer_operand(off)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				kind == TK_PTR ? "pointer indexing requires an integer index"
				: kind == TK_ARRAY ? "array indexing requires an integer index"
				: "slice indexing requires an integer index");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	TypeId index_type = finalize_type(s, index, off);
	n = s->ast->nodes + idx;

	if (kind == TK_ARRAY) {
		uint64_t bits;
		uint32_t count = types[ptr].count;

		if (lit_node_bits(s, s->ast->nodes + index, &bits)) {
			int neg = types[index_type].sign && (int64_t)bits < 0;

			if (neg || bits >= count)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"index %lld is out of bounds for an array of size %u",
						(long long)(int64_t)bits, count);
		}
	}

	n->data_type = types[ptr].base;
	if (kind == TK_PTR) warn_null_deref(s, idx, base);
	agg_member_read(s, idx);
	return n->data_type;
}
