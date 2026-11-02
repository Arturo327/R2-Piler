#include "sema/common.h"

#define MAX_EXPR_DEPTH 5000

static const uint8_t compound_base[NODE_COUNT] = {
	[NODE_ADD_ASSIGN] = NODE_ADD, [NODE_SUB_ASSIGN] = NODE_SUB,
	[NODE_MUL_ASSIGN] = NODE_MUL, [NODE_DIV_ASSIGN] = NODE_DIV,
	[NODE_MOD_ASSIGN] = NODE_MOD, [NODE_AND_A_ASSIGN] = NODE_AND_A,
	[NODE_OR_A_ASSIGN] = NODE_OR_A, [NODE_XOR_ASSIGN] = NODE_XOR,
	[NODE_LS_ASSIGN] = NODE_LS, [NODE_RS_ASSIGN] = NODE_RS
};

static TypeId check_literal (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	switch (n->type)
	{
	case NODE_LIT_i64:
		n->data_type = n->u64 > INT64_MAX ? TYPE_UNTYPED_UINT : TYPE_UNTYPED_INT;
		break;
	case NODE_LIT_u64: n->data_type = TYPE_UNTYPED_UINT; break;
	case NODE_LIT_CHAR: n->data_type = TYPE_UNTYPED_CHAR; break;
	case NODE_LIT_STR: n->data_type = TYPE_STR; break;
	default:
		n->data_type = TYPE_ERROR;
		break;
	}
	return n->data_type;
}

static TypeId check_init_ref (Sema *s, uint32_t idx, uint32_t sym_idx)
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

static TypeId check_var_ref (Sema *s, uint32_t idx, int init_refs)
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

	if (s->table.symbols[sym_idx].kind == SYMBOL_ENUM) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is an enum type, not a value", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (s->table.symbols[sym_idx].kind == SYMBOL_TYPE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is a type, not a variable", (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->sym = sym_idx;

	if (init_refs && s->init_node != NO_NODE
			&& s->table.symbols[sym_idx].kind == SYMBOL_VAR) {
		if (check_init_ref(s, idx, sym_idx) == TYPE_ERROR) return TYPE_ERROR;
	}

	n->data_type = s->table.symbols[sym_idx].type;
	return n->data_type;
}

static void warn_unassigned (Sema *s, uint32_t idx, TypeId type)
{
	ASTNode *n = s->ast->nodes + idx;

	if (type == TYPE_ERROR || n->sym == NO_SYMBOL) return;
	if (s->table.symbols[n->sym].assigned) return;

	error_report(s->err, ERR_WARNING, node_loc(n),
			"'%.*s' is used without being assigned", (int)n->len, n->str);
}

static void mark_read (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->sym == NO_SYMBOL) return;

	s->table.symbols[n->sym].read = 1;
	s->table.symbols[n->sym].pending_write = 0;
}

static void record_dead_write (Sema *s, uint32_t sym, uint32_t node)
{
	if (s->dead_count >= s->dead_cap) {
		uint32_t old_cap = s->dead_cap;

		s->dead_cap <<= 1;
		s->dead = arena_realloc(s->arena, s->dead,
				sizeof(DeadWrite) * old_cap, sizeof(DeadWrite) * s->dead_cap);
	}

	s->dead[s->dead_count].sym = sym;
	s->dead[s->dead_count].node = node;
	s->dead_count++;
}

void sym_write (Sema *s, uint32_t sym, uint32_t node)
{
	if (sym == NO_SYMBOL) return;

	Symbol *sp = &s->table.symbols[sym];
	if (sp->kind != SYMBOL_VAR && sp->kind != SYMBOL_PARAM) return;
	if (sp->depth == 0 || sp->addressed) return;

	if (sp->pending_write)
		record_dead_write(s, sym, sp->write_node);

	sp->pending_write = 1;
	sp->write_node = node;
}

static void set_null_state (Sema *s, uint32_t left, uint8_t left_kind, int is_null)
{
	if (left_kind != NODE_ID) return;
	uint32_t sym = s->ast->nodes[left].sym;
	if (sym != NO_SYMBOL) s->table.symbols[sym].null_known = is_null != 0;
}

static TypeId check_id (Sema *s, uint32_t idx)
{
	TypeId type = check_var_ref(s, idx, 1);

	warn_unassigned(s, idx, type);
	mark_read(s, idx);
	return type;
}

static void warn_null_deref (Sema *s, uint32_t idx, uint32_t child)
{
	ASTNode *n = s->ast->nodes + child;

	if (n->type != NODE_ID || n->sym == NO_SYMBOL) return;
	if (!s->table.symbols[n->sym].null_known) return;

	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"null pointer dereference: '%.*s' is NULL here",
			(int)n->len, n->str);
}

static TypeId check_deref (Sema *s, uint32_t idx)
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

static TypeId check_addr (Sema *s, uint32_t idx)
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
	} else {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of '$' must be a variable or a dereference");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (type == TYPE_ERROR) {
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
	case NODE_DEREF: {
		TypeId ptr;
		uint64_t ptr_size;

		if (!sizeof_expr_type(s, &s->ast->nodes[n->child], &ptr, &ptr_size))
			return 0;
		if (ptr == TYPE_ERROR) {
			*out = TYPE_ERROR;
			return 1;
		}
		if (type_kind(ptr) != TK_PTR) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot dereference a value of type %s", type_display_name(ptr));
			return 0;
		}
		if (types[ptr].base == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot dereference 'ptr void'; cast it to a typed pointer first");
			return 0;
		}
		*out = types[ptr].base;
		*size = types[*out].size;
		return 1;
	}
	default:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of sizeof must be a variable, dereference, string literal or type");
		return 0;
	}
}

static TypeId check_sizeof (Sema *s, uint32_t idx)
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

static uint32_t adapt_call_arg (Sema *s, uint32_t arg, TypeId arg_type, TypeId param_type)
{
	int ok;
	uint32_t new_arg = force_cast(s, arg, arg_type, param_type, &ok);

	if (ok) return new_arg;

	const char *hint = arg_type == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

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

static TypeId check_fn_call (Sema *s, uint32_t idx)
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

static TypeId check_dot (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	ASTNode *left = &s->ast->nodes[n->child];

	if (left->type != NODE_ID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"left side of '.' must be an enum type");
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

static inline int is_integer_operand (TypeId type)
{
	TypeKind kind = type_kind(type);

	return kind == TK_INT || kind == TK_UNTYPED_INT;
}

static inline int is_pointer_operand (TypeId type)
{
	return type_kind(type) == TK_PTR || type == TYPE_NULL || type == TYPE_STR;
}

static TypeId check_lvalue (Sema *s, uint32_t idx)
{
	switch (s->ast->nodes[idx].type)
	{
	case NODE_ID: return check_var_ref(s, idx, 1);
	case NODE_DEREF: return check_deref(s, idx);
	default: return TYPE_ERROR;
	}
}

static void mark_assigned (Sema *s, uint32_t left, uint8_t left_kind)
{
	uint32_t sym;

	if (left_kind == NODE_DEREF) sym = s->ast->nodes[s->ast->nodes[left].child].sym;
	else sym = s->ast->nodes[left].sym;

	if (sym != NO_SYMBOL) s->table.symbols[sym].assigned = 1;
}

static void warn_self_assign (Sema *s, uint32_t left, uint32_t right)
{
	ASTNode *l = s->ast->nodes + left;
	ASTNode *r = s->ast->nodes + right;

	if (l->type != NODE_ID || r->type != NODE_ID) return;
	if (l->sym == NO_SYMBOL || l->sym != r->sym) return;

	error_report(s->err, ERR_WARNING, node_loc(l),
			"'%.*s' is assigned to itself; this has no effect",
			(int)l->len, l->str);
}

static TypeId assign_type_error (Sema *s, uint32_t idx, TypeId l, TypeId r)
{
	const char *hint = r == TYPE_VOID
			? "a void value cannot be used here"
			: "use an explicit cast with 'as'";

	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"cannot assign %s to a variable of type %s; %s",
			type_display_name(r), types[l].name, hint);
	return TYPE_ERROR;
}

static TypeId check_assign (Sema *s, uint32_t idx)
{
	uint32_t left = s->ast->nodes[idx].child;
	uint32_t right = s->ast->nodes[left].next_bro;
	uint8_t left_kind = s->ast->nodes[left].type;

	if (left_kind != NODE_ID && left_kind != NODE_DEREF) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"left side of '=' must be a variable");
		check_expr(s, right);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	TypeId l = check_lvalue(s, left);
	TypeId r = check_expr(s, right);

	warn_self_assign(s, left, right);
	mark_assigned(s, left, left_kind);
	set_null_state(s, left, left_kind, is_null_value(s, right));
	if (left_kind == NODE_ID) sym_write(s, s->ast->nodes[left].sym, idx);

	if (l == TYPE_ERROR || r == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (l != r) {
		int ok;
		right = force_cast(s, right, r, l, &ok);
		s->ast->nodes[left].next_bro = right;

		if (!ok) return assign_type_error(s, idx, l, r);
	}

	s->ast->nodes[idx].data_type = l;
	return l;
}

static TypeId adapt_compound_rhs (Sema *s, uint32_t idx, uint32_t left, uint32_t right, TypeId l, TypeId r)
{
	uint8_t op = compound_base[s->ast->nodes[idx].type];

	if (op == NODE_LS || op == NODE_RS) {
		finalize_type(s, right, r);
		return l;
	}

	if (l != r) {
		int ok;
		uint32_t cast = force_cast(s, right, r, l, &ok);
		s->ast->nodes[left].next_bro = cast;
		if (!ok) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"cannot assign %s to a variable of type %s;"
					" use an explicit cast with 'as'",
					type_display_name(r), types[l].name);
			return TYPE_ERROR;
		}
		if (s->ast->nodes[cast].data_type == TYPE_ERROR) return TYPE_ERROR;
		right = cast;
	}

	uint64_t bits;
	if ((op == NODE_DIV || op == NODE_MOD)
			&& lit_node_bits(s, s->ast->nodes + right, &bits) && bits == 0) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + right),
				"division by zero");
		return TYPE_ERROR;
	}
	return l;
}

static TypeId check_compound_assign (Sema *s, uint32_t idx)
{
	uint32_t left = s->ast->nodes[idx].child;
	uint8_t left_kind = s->ast->nodes[left].type;
	uint32_t right = s->ast->nodes[left].next_bro;
	uint8_t op = compound_base[s->ast->nodes[idx].type];

	if (left_kind != NODE_ID && left_kind != NODE_DEREF) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"left side of compound assignment must be a variable");
		check_expr(s, right);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	TypeId l = check_lvalue(s, left);
	warn_unassigned(s, left, l);
	mark_read(s, left);
	TypeId r = check_expr(s, right);
	mark_assigned(s, left, left_kind);
	set_null_state(s, left, left_kind, 0);
	if (left_kind == NODE_ID) sym_write(s, s->ast->nodes[left].sym, idx);

	if (l == TYPE_ERROR || r == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (l == TYPE_VOID || r == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (type_kind(l) == TK_PTR) {
		if (op != NODE_ADD && op != NODE_SUB) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"operator cannot be applied to a value of type %s",
					types[l].name);
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		if (types[l].base == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		if (!is_integer_operand(r)) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer compound assignment requires an integer offset");
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		r = finalize_type(s, right, r);
		s->ast->nodes[idx].data_type = l;
		return l;
	}

	r = adapt_compound_rhs(s, idx, left, right, l, r);
	s->ast->nodes[idx].data_type = r == TYPE_ERROR ? TYPE_ERROR : l;
	return s->ast->nodes[idx].data_type;
}

void check_inc_dec (Sema *s, uint32_t idx)
{
	uint32_t target = s->ast->nodes[idx].child;
	uint8_t target_kind = s->ast->nodes[target].type;

	if (target_kind != NODE_ID && target_kind != NODE_DEREF) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operand of '%s' must be a variable",
				s->ast->nodes[idx].type == NODE_INC ? "++" : "--");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}

	TypeId type = check_lvalue(s, target);
	warn_unassigned(s, target, type);
	mark_read(s, target);
	mark_assigned(s, target, target_kind);
	set_null_state(s, target, target_kind, 0);
	if (target_kind == NODE_ID) sym_write(s, s->ast->nodes[target].sym, idx);

	if (type != TYPE_ERROR && type != TYPE_VOID
			&& type_kind(type) != TK_PTR) {
		s->ast->nodes[idx].data_type = type;
		return;
	}
	if (type == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}
	if (type_kind(type) == TK_PTR && types[type].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"pointer arithmetic on 'ptr void' is not allowed");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}

	s->ast->nodes[idx].data_type = type;
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

static TypeId check_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t op = n->type;
	TypeId operand_type = check_expr(s, n->child);
	TypeId result_type;
	int is_lit;

	n = &s->ast->nodes[idx];
	if (operand_type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (operand_type == TYPE_VOID || operand_type == TYPE_STR || (op != NODE_NOT_L
				&& (type_kind(operand_type) == TK_PTR || operand_type == TYPE_NULL))) {
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

static TypeId binop_result_type (uint8_t node_type, TypeId operand_type)
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
	case NODE_LIT_NULL: tru = 0; break;
	default: return;
	}

	error_report(s->err, ERR_WARNING, node_loc(n),
			"constant operand of '%s' is always %s", op, tru ? "true" : "false");
}

static TypeId check_rhs (Sema *s, uint8_t op, uint32_t right)
{
	uint32_t mark = s->table.act_count;

	if (op != NODE_AND_L && op != NODE_OR_L)
		return check_expr(s, right);

	uint8_t *snap = arena_alloc(s->arena, (size_t)mark + 1);
	symtab_save_flags(&s->table, snap, mark);
	symtab_clear_pending(&s->table);
	TypeId type = check_expr(s, right);
	symtab_intersect_flags(&s->table, snap, mark);
	symtab_load_flags(&s->table, snap, mark);
	return type;
}

static void coerce_string (Sema *s, uint32_t *idx, TypeId *type)
{
	if (*type != TYPE_STR) return;
	TypeId ptr = type_ptr_to(TYPE_i8);
	*idx = sema_wrap_cast(s, *idx, ptr);
	*type = ptr;
}

static void coerce_null (Sema *s, uint32_t *idx, TypeId *type, TypeId target)
{
	if (*type != TYPE_NULL) return;
	*idx = sema_wrap_cast(s, *idx, target);
	*type = target;
}

static TypeId pointer_mismatch (Sema *s, uint32_t idx, TypeId l, TypeId r)
{
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"pointer type mismatch: %s vs %s (use 'as' to convert explicitly)",
			types[l].name, types[r].name);
	return TYPE_ERROR;
}

static TypeId pointer_compare (Sema *s, uint32_t idx, uint32_t *left, uint32_t *right, TypeId *l, TypeId *r)
{
	coerce_string(s, left, l);
	coerce_string(s, right, r);

	if (type_kind(*l) == TK_PTR && type_kind(*r) == TK_PTR) {
		if (*l != *r) return pointer_mismatch(s, idx, *l, *r);
		return TYPE_i64;
	}
	if (type_kind(*l) == TK_PTR && *r == TYPE_NULL) {
		coerce_null(s, right, r, *l);
		return TYPE_i64;
	}
	if (*l == TYPE_NULL && type_kind(*r) == TK_PTR) {
		coerce_null(s, left, l, *r);
		return TYPE_i64;
	}
	if (*l == TYPE_NULL && *r == TYPE_NULL) return TYPE_i64;

	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"cannot compare a pointer with an integer;"
			" use NULL or an explicit cast with 'as'");
	return TYPE_ERROR;
}

static TypeId pointer_arith (Sema *s, uint32_t idx, uint8_t op, uint32_t *left, uint32_t *right, TypeId *l, TypeId *r)
{
	int both_str = *l == TYPE_STR && *r == TYPE_STR;

	coerce_string(s, left, l);
	coerce_string(s, right, r);

	if (type_kind(*l) == TK_PTR && is_integer_operand(*r)) {
		if (types[*l].base == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			return TYPE_ERROR;
		}
		*r = finalize_type(s, *right, *r);
		return *l;
	}
	if (op == NODE_ADD && is_integer_operand(*l)
			&& type_kind(*r) == TK_PTR) {
		if (types[*r].base == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			return TYPE_ERROR;
		}
		*l = finalize_type(s, *left, *l);
		return *r;
	}
	if (op == NODE_SUB && type_kind(*l) == TK_PTR
			&& type_kind(*r) == TK_PTR) {
		if (*l != *r) return pointer_mismatch(s, idx, *l, *r);
		if (types[*l].base == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			return TYPE_ERROR;
		}
		if (both_str)
			error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
					"subtracting two string literals is probably a mistake;"
					" the result is the byte distance between them");
		return TYPE_i64;
	}
	if (type_kind(*l) == TK_PTR && type_kind(*r) == TK_PTR) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"cannot add two pointers");
		return TYPE_ERROR;
	}
	if (*l == TYPE_NULL || *r == TYPE_NULL) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"cannot use NULL in pointer arithmetic");
		return TYPE_ERROR;
	}

	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"pointer arithmetic requires an integer offset");
	return TYPE_ERROR;
}

static int check_pointer_binary (Sema *s, uint32_t idx, uint32_t *left,
		uint32_t *right, TypeId *l, TypeId *r)
{
	uint8_t op = s->ast->nodes[idx].type;
	TypeId result;

	if (!is_pointer_operand(*l) && !is_pointer_operand(*r)) return 0;
	if (op == NODE_AND_L || op == NODE_OR_L) return 0;

	switch (op)
	{
	case NODE_EQ: case NODE_NE: case NODE_GT: case NODE_GE:
	case NODE_LT: case NODE_LE:
		result = pointer_compare(s, idx, left, right, l, r);
		break;
	case NODE_ADD: case NODE_SUB:
		result = pointer_arith(s, idx, op, left, right, l, r);
		break;
	default: {
		TypeId bad = is_pointer_operand(*l) ? *l : *r;

		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[bad].name);
		result = TYPE_ERROR;
		break;
	}
	}

	if (result == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return 1;
	}
	s->ast->nodes[idx].child = *left;
	s->ast->nodes[*left].next_bro = *right;
	s->ast->nodes[idx].data_type = result;
	return 1;
}

static int check_logic (Sema *s, uint32_t idx, uint32_t left, uint32_t right, TypeId *l, TypeId *r)
{
	const char *op = s->ast->nodes[idx].type == NODE_AND_L ? "&&" : "||";
	TypeId bad = *l == TYPE_STR ? *l : *r;

	if (bad == TYPE_STR) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[bad].name);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return 0;
	}
	warn_const_operand(s, left, op);
	warn_const_operand(s, right, op);
	*l = finalize_type(s, left, *l);
	*r = finalize_type(s, right, *r);
	return 1;
}

static TypeId check_binary (Sema *s, uint32_t idx)
{
	uint8_t op_type = s->ast->nodes[idx].type;
	uint32_t left = s->ast->nodes[idx].child;
	uint32_t right = s->ast->nodes[left].next_bro;
	TypeId l = check_expr(s, left);
	TypeId r = check_rhs(s, op_type, right);

	if (l == TYPE_ERROR || r == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (l == TYPE_VOID || r == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (op_type != NODE_AND_L && op_type != NODE_OR_L
			&& check_pointer_binary(s, idx, &left, &right, &l, &r))
		return s->ast->nodes[idx].data_type;

	if (op_type == NODE_AND_L || op_type == NODE_OR_L) {
		if (!check_logic(s, idx, left, right, &l, &r)) return TYPE_ERROR;
	} else if (op_type == NODE_LS || op_type == NODE_RS) {
		finalize_type(s, right, r);
		if (!is_literal_node(s, right))
			l = finalize_type(s, left, l);
	}

	if (l != r && needs_common_type(op_type)
			&& !unify_operands(s, s->ast->nodes + idx, &left, &right, &l, &r)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	s->ast->nodes[idx].child = left;
	s->ast->nodes[left].next_bro = right;
	s->ast->nodes[idx].data_type = binop_result_type(op_type, l);
	try_fold_binary(s, idx);
	return s->ast->nodes[idx].data_type;
}

static int cast_types_ok (Sema *s, ASTNode *n, TypeId from, TypeId to)
{
	if (from == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast a value of type void");
		return 0;
	}
	if ((is_pointer_operand(from) || is_pointer_operand(to))
			&& !(is_pointer_operand(from) && is_pointer_operand(to))) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast between pointer and non-pointer types;"
				" only pointer-to-pointer casts are supported");
		return 0;
	}
	if (type_kind(from) == TK_ENUM && type_kind(to) == TK_ENUM && from != to) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast between enum types '%s' and '%s';"
				" cast through the underlying integer type",
				types[from].name, types[to].name);
		return 0;
	}
	if (type_kind(from) == TK_NEWTYPE && type_kind(to) == TK_NEWTYPE
			&& from != to) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast between newtype types '%s' and '%s';"
				" cast through the underlying integer type",
				types[from].name, types[to].name);
		return 0;
	}
	return 1;
}

static TypeId check_cast (Sema *s, uint32_t idx)
{
	TypeId from = check_expr(s, s->ast->nodes[idx].child);
	TypeId to = resolve_type_node(s, s->ast->nodes + idx);

	ASTNode *n = s->ast->nodes + idx;
	if (from == TYPE_ERROR || to == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (!cast_types_ok(s, n, from, to)) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (from == to)
		error_report(s->err, ERR_WARNING, node_loc(n),
				"redundant cast to '%s'; the value already has that type",
				types[to].name);

	finalize_type(s, n->child, from);
	return n->data_type;
}

static TypeId check_expr_node (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64:
	case NODE_LIT_CHAR: case NODE_LIT_STR:
		return check_literal(s, idx);
	case NODE_ID: return check_id(s, idx);
	case NODE_LIT_NULL: n->data_type = TYPE_NULL; return TYPE_NULL;
	case NODE_DEREF: return check_deref(s, idx);
	case NODE_ADDR: return check_addr(s, idx);
	case NODE_SIZEOF: return check_sizeof(s, idx);
	case NODE_DOT: return check_dot(s, idx);
	case NODE_FN_CALL: return check_fn_call(s, idx);
	case NODE_ASSIGN: return check_assign(s, idx);
	case NODE_ADD_ASSIGN: case NODE_SUB_ASSIGN: case NODE_MUL_ASSIGN:
	case NODE_DIV_ASSIGN: case NODE_MOD_ASSIGN: case NODE_AND_A_ASSIGN:
	case NODE_OR_A_ASSIGN: case NODE_XOR_ASSIGN: case NODE_LS_ASSIGN:
	case NODE_RS_ASSIGN: return check_compound_assign(s, idx);
	case NODE_INC: case NODE_DEC:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"increment/decrement cannot be used as an expression");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	case NODE_NEG: case NODE_NOT_L: case NODE_NOT_A:
		return check_unary(s, idx);
	case NODE_CAST: return check_cast(s, idx);
	case NODE_EMPTY: n->data_type = TYPE_VOID; return TYPE_VOID;
	case NODE_ERROR: n->data_type=TYPE_ERROR; return TYPE_ERROR;
	default: return check_binary(s, idx);
	}
}

TypeId check_expr (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (s->expr_depth >= MAX_EXPR_DEPTH) {
		if (!s->too_deep)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"expression or initializer chain nested too deeply");
		s->too_deep = 1;
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	s->expr_depth++;
	TypeId type = check_expr_node(s, idx);
	s->expr_depth--;
	return type;
}

int expr_has_effect (Sema *s, uint32_t idx)
{
	ASTNode *n;
	uint32_t child;

	if (idx == NO_NODE) return 0;
	n = s->ast->nodes + idx;
	child = n->child;

	switch (n->type)
	{
	case NODE_FN_CALL: case NODE_INC: case NODE_DEC:
	case NODE_ASSIGN: return 1;
	case NODE_LIT_i64: case NODE_LIT_u64: case NODE_LIT_CHAR:
	case NODE_LIT_STR: case NODE_LIT_NULL: case NODE_EMPTY:
		return 0;
	case NODE_ID: case NODE_DEREF: case NODE_ADDR:
	case NODE_NEG: case NODE_NOT_L: case NODE_NOT_A:
	case NODE_SIZEOF: case NODE_CAST:
		return expr_has_effect(s, child);
	default:
		break;
	}

	if (compound_base[n->type] != 0 || child == NO_NODE) return 1;
	return expr_has_effect(s, child)
			|| expr_has_effect(s, s->ast->nodes[child].next_bro);
}
