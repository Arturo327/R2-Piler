#include "sema/common.h"

#include <string.h>

static int is_static_const (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint64_t bits;

	switch (n->type)
	{
	case NODE_LIT_NULL: case NODE_LIT_STR:
		return 1;
	case NODE_ID:
		return n->sym != NO_SYMBOL && s->table.symbols[n->sym].kind == SYMBOL_FN;
	case NODE_CAST: case NODE_FIELD_INIT:
		return n->child != NO_NODE && is_static_const(s, n->child);
	case NODE_INDEX_INIT:
		return is_static_const(s, (uint32_t)n->u64);
	case NODE_ARRAY_INIT: case NODE_STRUCT_INIT: {
		uint32_t item = n->child;

		while (item != NO_NODE) {
			if (!is_static_const(s, item)) return 0;
			item = s->ast->nodes[item].next_bro;
		}
		return 1;
	}
	case NODE_ADDR: {
		ASTNode *c = n->child != NO_NODE ? &s->ast->nodes[n->child] : NULL;

		return c != NULL && c->type == NODE_ID && c->sym != NO_SYMBOL
				&& s->table.symbols[c->sym].depth == 0;
	}
	default:
		return lit_node_bits(s, n, &bits);
	}
}

static void check_default_sym (Sema *s, uint32_t node, const char *param, uint16_t plen)
{
	ASTNode *n = s->ast->nodes + node;

	if (n->type != NODE_ID || n->sym == NO_SYMBOL) return;
	Symbol *sym = s->table.symbols + n->sym;

	if (sym->decl != NO_NODE && !(s->ast->nodes[sym->decl].flags
			& (NODE_FLAG_PUB | NODE_FLAG_EXPORT)))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"default argument for parameter '%.*s' exposes non-public symbol '%.*s'",
				(int)plen, param, (int)sym->len, sym->name);
}

static void check_default_syms (Sema *s, uint32_t node, const char *param, uint16_t plen)
{
	ASTNode *n = s->ast->nodes + node;

	check_default_sym(s, node, param, plen);
	if (n->type == NODE_INDEX_INIT && n->u64 != NO_NODE)
		check_default_syms(s, (uint32_t)n->u64, param, plen);
	for (uint32_t c = n->child; c != NO_NODE; c = s->ast->nodes[c].next_bro)
		check_default_syms(s, c, param, plen);
}

static int new_init (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	while (n->type == NODE_CAST && n->child != NO_NODE)
		n = s->ast->nodes + n->child;
	return n->type == NODE_NEW;
}

static int refs_local (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->type == NODE_ID && n->sym != NO_SYMBOL
			&& s->table.symbols[n->sym].depth > 0)
		return 1;
	for (uint32_t c = n->child; c != NO_NODE; c = s->ast->nodes[c].next_bro) {
		if (refs_local(s, c)) return 1;
	}
	return 0;
}

static void check_static_decl (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	int has_init = n->child != NO_NODE;

	if (has_init && n->data_type != TYPE_ERROR) {
		if (new_init(s, n->child)) {
			if (refs_local(s, n->child))
				error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + n->child),
						"a 'static' initializer with 'new' cannot reference locals");
		} else if (!is_static_const(s, n->child)) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + n->child),
					"a 'static' initializer must be a constant expression");
		}
	}

	uint32_t sym = sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);

	s->table.symbols[sym].is_static = 1;
	s->table.symbols[sym].assigned = 1;
	s->table.symbols[sym].owner = s->curr_fn;
	if (has_init) record_init_order(s, idx);
	record_static(s, idx);
}

static TypeId infer_decl_type (Sema *s, uint32_t idx, TypeId init)
{
	ASTNode *n = s->ast->nodes + idx;

	if (init == TYPE_ERROR) return TYPE_ERROR;
	if (init == TYPE_NULL) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot infer the type of '%.*s' from NULL; add an explicit type",
				(int)n->len, n->str);
		return TYPE_ERROR;
	}
	if (init == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot infer the type of '%.*s' from a void value; add an explicit type",
				(int)n->len, n->str);
		return TYPE_ERROR;
	}
	if (init == TYPE_STR) {
		TypeId ptr = type_ptr_to(TYPE_i8);

		if (ptr == TYPE_ERROR)
			error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return ptr;
	}
	if (type_kind(init) == TK_ARRAY) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot infer an array type for '%.*s'; add an explicit array type",
				(int)n->len, n->str);
		return TYPE_ERROR;
	}
	return finalize_type(s, n->child, init);
}

static void check_inferred_decl (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->type == NODE_ARR_DEC) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"untyped 'arr' declarations are not allowed; add the element type");
		n->data_type = TYPE_ERROR;
		return;
	}
	if (n->child == NO_NODE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot infer the type of '%.*s'; add an explicit type or an initializer",
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return;
	}

	TypeId init = check_expr(s, n->child);
	TypeId decl = infer_decl_type(s, idx, init);

	n = s->ast->nodes + idx;
	n->data_type = decl;
	if (decl != TYPE_ERROR && decl != init)
		check_var_init_type(s, n, init);
}

static void check_view_init (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint8_t it = s->ast->nodes[n->child].type;

	if (it == NODE_ARRAY_INIT || it == NODE_STRUCT_INIT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + n->child),
				"a view cannot be initialized with a '{...}' list;"
				" use an array or slice expression");
		return;
	}
	TypeId init = check_expr(s, n->child);

	n = s->ast->nodes + idx;
	if (init == TYPE_ERROR || n->data_type == TYPE_ERROR) return;
	if (sema_array_slice_compat(init, n->data_type)) return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot initialize view '%.*s' (%s) with a value of type %s;"
			" a view requires an array or slice of the same rank and element type,"
			" with the same size in every sized dimension",
			(int)n->len, n->str, types[n->data_type].name,
			type_display_name(init));
}

static void check_decl_init (Sema *s, uint32_t idx, int is_view)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeKind kind = n->data_type == TYPE_ERROR ? TK_ERROR : type_kind(n->data_type);

	if (is_view) {
		check_view_init(s, idx);
	} else if (kind == TK_ARRAY) {
		check_array_init(s, idx);
	} else if ((kind == TK_STRUCT || kind == TK_CLASS) && (s->ast->nodes[n->child].type == NODE_STRUCT_INIT
			|| s->ast->nodes[n->child].type == NODE_ARRAY_INIT)) {
		check_struct_init(s, n->child, n->data_type);
	} else if (s->ast->nodes[n->child].type == NODE_STRUCT_INIT) {
		if (kind != TK_ERROR)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + n->child),
					"'{.field = ...}' initializer requires a struct type");
	} else if (s->ast->nodes[n->child].type == NODE_ARRAY_INIT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + n->child),
				"'{...}' initializer requires an array type");
	} else {
		check_var_init_type(s, n, check_expr(s, n->child));
	}
}

void check_var_dec (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	int untyped = (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC)
			&& n->type_node == NO_TYPE_NODE && n->data_type == TYPE_VOID;
	int has_init = n->child != NO_NODE;
	int is_null = has_init && is_null_value(s, n->child);

	if (has_init) infer_array_dims(s, idx);
	resolve_type_node(s, n);
	n = s->ast->nodes + idx;
	if (untyped) check_inferred_decl(s, idx);
	else check_storage_decl(s, n);
	check_const_init(s, s->ast->nodes + idx);

	n = s->ast->nodes + idx;
	int is_view = n->type == NODE_VAR_DEC && !(n->flags & NODE_FLAG_STATIC)
			&& n->data_type != TYPE_ERROR && type_kind(n->data_type) == TK_ARRAY;

	if (is_view && !has_init && n->str[0] != '_')
		error_report(s->err, ERR_WARNING, node_loc(n),
				"variable '%.*s' is a view with no initializer; it reserves no storage"
				" (use 'arr' for a stack array)", (int)n->len, n->str);

	if (has_init && !untyped) check_decl_init(s, idx, is_view);

	n = s->ast->nodes + idx;
	if (n->flags & NODE_FLAG_STATIC) {
		check_static_decl(s, idx);
		return;
	}
	uint32_t sym = sema_declare(s, n->str, n->len, SYMBOL_VAR, n->data_type, idx);

	if (is_null) s->table.symbols[sym].null_known = 1;
	if (has_init) {
		sym_set_stack_slice(s, sym, s->ast->nodes[idx].child);
		sym_write(s, sym, idx);
	}
}

void check_main_signature (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	int bad_params = s->ast->nodes[args].child != NO_NODE;
	int bad_ret = s->ast->nodes[ret].data_type != TYPE_i64
			|| s->ast->nodes[ret].child != NO_NODE;

	if (n->len != 4 || memcmp(n->str, "main", 4) != 0)
		return;
	if (n->flags & NODE_FLAG_EXPORT)
		error_report(s->err, ERR_ERROR, node_loc(n), "'main' cannot be exported");
	else if (!(n->flags & NODE_FLAG_PUB) || bad_params || bad_ret)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'main' must be declared as 'pub fn main() : i64'");
}

static void check_ret_type (Sema *s, uint32_t ret, int is_extern, int multi)
{
	ASTNode *n = s->ast->nodes + ret;
	TypeId t = n->data_type;

	if (t == TYPE_ERROR) return;
	if (multi && t == TYPE_VOID)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"void cannot be used as one of several return types");
	if (!is_extern || t == TYPE_VOID) return;

	if (type_is_struct_like(t))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"extern functions cannot return structs by value yet");
	else if (type_kind(t) == TK_ARRAY || type_kind(t) == TK_SLICE)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"extern functions cannot return arrays or slices yet");
}

void check_signature_types (Sema *s, uint32_t idx, int is_extern)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t item = s->ast->nodes[ret].child;

	check_ret_type(s, ret, is_extern, item != NO_NODE);
	while (item != NO_NODE) {
		check_ret_type(s, item, is_extern, 1);
		item = s->ast->nodes[item].next_bro;
	}
	int typed = (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED) != 0;

	if (!is_extern && (s->ast->nodes[args].flags & NODE_FLAG_VARARG))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"variadic parameters ('...') are only allowed in extern functions");
	if (is_extern && typed)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"typed variadic parameters ('name : ...T') are only allowed in non-extern functions");

	uint32_t p = s->ast->nodes[args].child;
	while (p != NO_NODE) {
		TypeId type = s->ast->nodes[p].data_type;

		if (typed && s->ast->nodes[p].next_bro == NO_NODE) break;
		if (is_extern && type_dyn_rank(type) > 0)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"extern functions cannot take slice parameters");
		if (is_extern && type_is_struct_like(type))
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"extern functions cannot take structs by value yet");

		p = s->ast->nodes[p].next_bro;
	}
}

static void check_export_ret (Sema *s, uint32_t ret)
{
	ASTNode *n = s->ast->nodes + ret;
	TypeId t = n->data_type;

	if (t == TYPE_ERROR) return;
	if (type_is_struct_like(t))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"export functions cannot return structs by value yet");
	else if (type_kind(t) == TK_ARRAY || type_dyn_rank(t) > 0)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"export functions cannot return arrays or slices yet");
}

void check_export_fn (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;

	if (!(n->flags & NODE_FLAG_EXPORT)) return;
	if (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"export functions cannot be variadic");
		return;
	}
	if (s->ast->nodes[ret].child != NO_NODE)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"export functions cannot return multiple values yet");
	else check_export_ret(s, ret);

	uint32_t p = s->ast->nodes[args].child;
	while (p != NO_NODE) {
		TypeId type = s->ast->nodes[p].data_type;

		if (type_is_struct_like(type))
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"export functions cannot take structs by value yet");
		else if (type_dyn_rank(type) > 0)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"export functions cannot take slice parameters");
		p = s->ast->nodes[p].next_bro;
	}
}

static void check_param_default (Sema *s, uint32_t param, int is_public)
{
	ASTNode *p = s->ast->nodes + param;
	uint32_t def = p->child;
	TypeId type = p->data_type;

	if (def == NO_NODE || type == TYPE_ERROR) return;
	if (p->flags & NODE_FLAG_SELF) return;
	if (p->flags & NODE_FLAG_REF_LOWERED) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + def),
				"a reference parameter cannot have a default value");
		return;
	}
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + def),
				"an array or slice parameter cannot have a default value");
		return;
	}

	TypeId def_type = check_expr(s, def);
	if (def_type == TYPE_ERROR) return;
	if (!is_static_const(s, def)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + def),
				"a default argument must be a constant expression");
		return;
	}

	int ok;
	const char *pname = p->str;
	uint16_t plen = p->len;
	uint32_t cast = force_cast(s, def, def_type, type, &ok);
	s->ast->nodes[param].child = cast;
	if (!ok)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cast),
				"default argument type %s does not match parameter type %s; use an explicit cast with 'as'",
				type_display_name(def_type), types[type].name);
	else if (is_public) check_default_syms(s, def, pname, plen);
}

static void check_fn_defaults (Sema *s, uint32_t idx)
{
	uint32_t args = s->ast->nodes[idx].child;
	uint32_t param = s->ast->nodes[args].child;
	int seen = 0;
	int is_public = (s->ast->nodes[idx].flags
			& (NODE_FLAG_PUB | NODE_FLAG_EXPORT)) != 0;

	if (s->ast->nodes[idx].type == NODE_FN_DEC
			&& (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED)) {
		while (param != NO_NODE && s->ast->nodes[param].child == NO_NODE)
			param = s->ast->nodes[param].next_bro;
		if (param != NO_NODE)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + param),
					"a variadic function cannot have default arguments");
		return;
	}

	while (param != NO_NODE) {
		ASTNode *p = s->ast->nodes + param;
		int has_default = p->child != NO_NODE;

		if (has_default) seen = 1;
		else if (seen)
			error_report(s->err, ERR_ERROR, node_loc(p),
					"required parameter '%.*s' cannot follow an optional parameter",
					(int)p->len, p->str);
		check_param_default(s, param, is_public);
		param = s->ast->nodes[param].next_bro;
	}
}

void check_fn_defaults_all (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_FN_DEC || n->type == NODE_EXTERN_FN_DEC) {
			s->in_imported = (n->flags & NODE_FLAG_IMPORTED) != 0;
			check_fn_defaults(s, idx);
			s->in_imported = 0;
		} else if (n->type == NODE_CLASS_DEC && n->sym != NO_SYMBOL) {
			TypeId saved = s->curr_class;

			s->curr_class = s->table.symbols[n->sym].type;
			for (uint32_t m = n->child; m != NO_NODE; m = s->ast->nodes[m].next_bro)
				if (s->ast->nodes[m].type == NODE_FN_DEC)
					check_fn_defaults(s, m);
			s->curr_class = saved;
		}
		idx = n->next_bro;
	}
}

