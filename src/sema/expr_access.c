#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

TypeId check_index (Sema *s, uint32_t idx);

static int sizeof_named_type (Sema *s, ASTNode *n, TypeId *out)
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
				"operand of sizeof cannot be a function");
		return 0;
	}
	if (sym->kind != SYMBOL_VAR && sym->kind != SYMBOL_PARAM
			&& sym->kind != SYMBOL_TYPE && sym->kind != SYMBOL_ENUM)
		return 0;

	*out = sym->type;
	return 1;
}

static int sizeof_expr_type (Sema *s, ASTNode *n, TypeId *out, uint64_t *size)
{
	switch (n->type)
	{
	case NODE_ID:
		if (!sizeof_named_type(s, n, out)) return 0;
		if (*out != TYPE_ERROR) *size = types[*out].size;
		return 1;
	case NODE_LIT_STR:
		*out = TYPE_STR;
		*size = (uint64_t)n->len + 1;
		return 1;
	case NODE_LIT_NULL:
		*out = TYPE_NULL;
		*size = types[TYPE_NULL].size;
		return 1;
	case NODE_LIT_i64: *out = TYPE_i64; *size = 8; return 1;
	case NODE_LIT_u64: *out = TYPE_u64; *size = 8; return 1;
	case NODE_LIT_CHAR: *out = TYPE_i8; *size = 1; return 1;
	case NODE_DEREF:
	case NODE_INDEX: {
		const char *op = n->type == NODE_INDEX ? "index" : "dereference";
		TypeId ptr;
		uint64_t ptr_size;

		if (!sizeof_expr_type(s, &s->ast->nodes[n->child], &ptr, &ptr_size))
			return 0;
		if (ptr == TYPE_ERROR) {
			*out = TYPE_ERROR;
			return 1;
		}
		TypeKind kind = type_kind(ptr);
		if (kind == TK_ARRAY || kind == TK_SLICE) {
			*out = types[ptr].base;
			*size = types[*out].size;
			return 1;
		}
		if (kind != TK_PTR) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot %s a value of type %s", op, type_display_name(ptr));
			return 0;
		}
		if (types[ptr].base == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot %s 'ptr void'; cast it to a typed pointer first", op);
			return 0;
		}
		*out = types[ptr].base;
		*size = types[*out].size;
		return 1;
	}
	default:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of sizeof must be a variable, dereference, index, string literal or type");
		return 0;
	}
}

TypeId check_sizeof (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	TypeId type;
	uint64_t size;

	if (n->child == NO_NODE) {
		type = resolve_type_node(s, n);
		if (type == TYPE_ERROR) {
			n->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		if (type == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"sizeof cannot be applied to type void");
			n->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		size = types[type].size;
	} else {
		if (!sizeof_expr_type(s, &s->ast->nodes[n->child], &type, &size)) {
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		if (type == TYPE_ERROR) {
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		if (type == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"sizeof cannot be applied to a value of type void");
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}

	n = &s->ast->nodes[idx];
	n->type = NODE_LIT_i64;
	n->i64 = (int64_t)size;
	n->data_type = TYPE_UNTYPED_INT;
	n->child = NO_NODE;
	n->type_node = NO_TYPE_NODE;
	n->sym = NO_SYMBOL;
	return TYPE_UNTYPED_INT;
}

TypeId check_deref (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	TypeId type = check_expr(s, n->child);

	n = &s->ast->nodes[idx];
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
	ASTNode *n = &s->ast->nodes[idx];
	ASTNode *child = &s->ast->nodes[n->child];
	TypeId type;

	if (child->type == NODE_ID) {
		type = check_var_ref(s, n->child, 0);
		n = &s->ast->nodes[idx];
		child = &s->ast->nodes[n->child];
		if (type != TYPE_ERROR && child->sym != NO_SYMBOL)
			s->table.symbols[child->sym].addressed = 1;
		mark_read(s, n->child);
	} else if (child->type == NODE_DEREF) {
		type = check_deref(s, n->child);
		n = &s->ast->nodes[idx];
	} else if (child->type == NODE_INDEX) {
		type = check_index(s, n->child);
		n = &s->ast->nodes[idx];
	} else {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of '$' must be a variable, a dereference or an index");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

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
	return n->data_type;
}

static int array_slice_compat (TypeId arg, TypeId param)
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

static void check_call_args (Sema *s, uint32_t call_idx, uint32_t fn_idx)
{
	ASTNode *call = &s->ast->nodes[call_idx];
	uint32_t params_node = s->ast->nodes[fn_idx].child;
	uint32_t param = s->ast->nodes[params_node].child;
	uint32_t arg = call->child;
	uint32_t prev = NO_NODE;
	int count_ok = 1;

	while (arg != NO_NODE) {
		TypeId arg_type = check_expr(s, arg);
		TypeId param_type = param != NO_NODE ? s->ast->nodes[param].data_type : TYPE_VOID;
		uint32_t next = s->ast->nodes[arg].next_bro;

		if (param == NO_NODE) {
			count_ok = 0;
		} else if (arg_type != TYPE_ERROR && param_type != TYPE_ERROR && arg_type != param_type
				&& !array_slice_compat(arg_type, param_type)) {
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

TypeId check_fn_call (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
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

static TypeId check_dot_index(Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_index(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE)
		return check_len_field(s, idx, n->child, type);

	error_report(s->err, ERR_ERROR, node_loc(n),
			"a value of type %s is not an array or a slice", type_display_name(type));
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

static TypeId check_dot_var(Sema *s, uint32_t idx, Symbol *sym)
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

	n = &s->ast->nodes[idx];
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return check_len_field(s, idx, n->child, type);
}

static TypeId check_dot_enum(Sema *s, uint32_t idx, Symbol *sym)
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

TypeId check_dot(Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *left = s->ast->nodes + n->child;

	if (left->type == NODE_INDEX)
		return check_dot_index(s, idx);

	if (left->type != NODE_ID) {
		TypeId left_type = check_expr(s, n->child);

		n = s->ast->nodes + idx;
		if (left_type == TYPE_ERROR) {
			n->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		if (n->len == 3 && memcmp(n->str, "len", 3) == 0)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"a value of type %s is not an array or a slice",
					type_display_name(left_type));
		else
			error_report(s->err, ERR_ERROR, node_loc(n),
					"a value of type %s is not an enum type",
					type_display_name(left_type));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	uint32_t sym_idx = symtab_find(&s->table, left->str, left->len);
	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(left),
				"'%.*s' is not declared", (int)left->len, left->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	Symbol *sym = s->table.symbols + sym_idx;
	if (sym->kind == SYMBOL_VAR || sym->kind == SYMBOL_PARAM)
		return check_dot_var(s, idx, sym);

	return check_dot_enum(s, idx, sym);
}

TypeId check_index (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint32_t base = n->child;
	uint32_t index = s->ast->nodes[base].next_bro;
	TypeId ptr = check_expr(s, base);
	TypeId off = check_expr(s, index);

	n = &s->ast->nodes[idx];
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
	n = &s->ast->nodes[idx];

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
	return n->data_type;
}
