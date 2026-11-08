#include "sema/common.h"
#include "sema/expr_internal.h"

TypeId check_literal (Sema *s, uint32_t idx)
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

TypeId check_var_ref (Sema *s, uint32_t idx, int init_refs)
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

void mark_read (Sema *s, uint32_t idx)
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
	if (sp->depth == 0 || sp->addressed || sp->is_static) return;

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

TypeId check_id (Sema *s, uint32_t idx)
{
	TypeId type = check_var_ref(s, idx, 1);

	warn_unassigned(s, idx, type);
	mark_read(s, idx);
	return type;
}

void warn_null_deref (Sema *s, uint32_t idx, uint32_t child)
{
	ASTNode *n = s->ast->nodes + child;

	if (n->type != NODE_ID || n->sym == NO_SYMBOL) return;
	if (!s->table.symbols[n->sym].null_known) return;

	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"null pointer dereference: '%.*s' is NULL here",
			(int)n->len, n->str);
}

static TypeId check_lvalue (Sema *s, uint32_t idx)
{
	switch (s->ast->nodes[idx].type)
	{
	case NODE_ID: return check_var_ref(s, idx, 1);
	case NODE_DEREF: return check_deref(s, idx);
	case NODE_INDEX: return check_index(s, idx);
	case NODE_DOT: return check_dot(s, idx);
	default: return TYPE_ERROR;
	}
}

static TypeId reject_place (Sema *s, uint32_t idx, uint32_t right, const char *what)
{
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"%s must be a variable", what);
	if (right != NO_NODE) check_expr(s, right);
	s->ast->nodes[idx].data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

static void mark_assigned (Sema *s, uint32_t left)
{
	ASTNode *n = &s->ast->nodes[left];

	while (n->type == NODE_DEREF || n->type == NODE_INDEX || n->type == NODE_DOT)
		n = &s->ast->nodes[n->child];
	if (n->type == NODE_ID && n->sym != NO_SYMBOL)
		s->table.symbols[n->sym].assigned = 1;
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

TypeId check_assign (Sema *s, uint32_t idx)
{
	uint32_t left = s->ast->nodes[idx].child;
	uint32_t right = s->ast->nodes[left].next_bro;
	uint8_t left_kind = s->ast->nodes[left].type;

	if (left_kind != NODE_ID && left_kind != NODE_DEREF &&
			left_kind != NODE_INDEX && left_kind != NODE_DOT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"left side of '=' must be a variable");
		check_expr(s, right);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (left_kind == NODE_DOT) {
		ASTNode *dot_left = s->ast->nodes + s->ast->nodes[left].child;
		if (dot_left->type == NODE_ID) {
			uint32_t sym_idx = symtab_find(&s->table, dot_left->str, dot_left->len);
			if (sym_idx != NO_SYMBOL) {
				TypeId ct = s->table.symbols[sym_idx].type;
				if (type_kind(ct) == TK_ARRAY || type_kind(ct) == TK_SLICE) {
					error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
							"left side of '=' must be a variable");
					check_expr(s, right);
					s->ast->nodes[idx].data_type = TYPE_ERROR;
					return TYPE_ERROR;
				}
			}
		}
	}

	TypeId l = check_lvalue(s, left);

	if (left_kind == NODE_DOT && s->ast->nodes[left].type != NODE_DOT)
		return reject_place(s, idx, right, "left side of '='");
	TypeId r = check_expr(s, right);

	warn_self_assign(s, left, right);
	mark_assigned(s, left);
	set_null_state(s, left, left_kind, is_null_value(s, right));
	if (left_kind == NODE_ID) sym_write(s, s->ast->nodes[left].sym, idx);

	if (l == TYPE_ERROR || r == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	int slice_field = left_kind == NODE_DOT && type_dyn_rank(l) > 0;
	if ((type_kind(l) == TK_ARRAY || type_kind(l) == TK_SLICE) && !slice_field) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"cannot assign a value of type %s; arrays and slices"
				" must be modified element by element", types[l].name);
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

typedef struct {
	TypeId l;
	TypeId r;
} CompoundLhsResult;

static int reject_compound_dot_array_slice (Sema *s, uint32_t idx, uint32_t left,
		uint32_t right)
{
	ASTNode *dot_left = s->ast->nodes + s->ast->nodes[left].child;
	if (dot_left->type != NODE_ID) return 0;
	uint32_t sym_idx = symtab_find(&s->table, dot_left->str, dot_left->len);
	if (sym_idx == NO_SYMBOL) return 0;
	TypeId ct = s->table.symbols[sym_idx].type;
	if (type_kind(ct) != TK_ARRAY && type_kind(ct) != TK_SLICE) return 0;
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"left side of compound assignment must be a variable");
	check_expr(s, right);
	return 1;
}

static int validate_compound_left (Sema *s, uint32_t idx, uint32_t left,
		uint8_t left_kind, uint32_t right)
{
	if (left_kind != NODE_ID && left_kind != NODE_DEREF
			&& left_kind != NODE_INDEX && left_kind != NODE_DOT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"left side of compound assignment must be a variable");
		check_expr(s, right);
		return 0;
	}
	if (left_kind == NODE_DOT && reject_compound_dot_array_slice(s, idx, left, right))
		return 0;
	return 1;
}

static CompoundLhsResult check_compound_lhs (Sema *s, uint32_t idx, uint32_t left,
		uint8_t left_kind, uint32_t right)
{
	CompoundLhsResult res;

	res.l = check_lvalue(s, left);

	if (left_kind == NODE_DOT && s->ast->nodes[left].type != NODE_DOT) {
		res.l = reject_place(s, idx, right, "left side of compound assignment");
		res.r = TYPE_ERROR;
		return res;
	}
	warn_unassigned(s, left, res.l);
	mark_read(s, left);
	res.r = check_expr(s, right);
	mark_assigned(s, left);
	set_null_state(s, left, left_kind, 0);
	if (left_kind == NODE_ID) sym_write(s, s->ast->nodes[left].sym, idx);
	return res;
}

static int check_compound_type_valid (Sema *s, uint32_t idx, TypeId l, TypeId r)
{
	if (l == TYPE_ERROR || r == TYPE_ERROR) return 0;
	if (type_is_aggregate(l)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[l].name);
		return 0;
	}
	if (l == TYPE_VOID || r == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		return 0;
	}
	return 1;
}

static TypeId check_compound_ptr (Sema *s, uint32_t idx, uint8_t op,
		TypeId l, TypeId r, uint32_t right)
{
	if (op != NODE_ADD && op != NODE_SUB) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[l].name);
		return TYPE_ERROR;
	}
	if (types[l].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"pointer arithmetic on 'ptr void' is not allowed");
		return TYPE_ERROR;
	}
	if (!is_integer_operand(r)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"pointer compound assignment requires an integer offset");
		return TYPE_ERROR;
	}
	r = finalize_type(s, right, r);
	return l;
}

static TypeId check_compound_regular (Sema *s, uint32_t idx, uint32_t left,
		TypeId l, TypeId r, uint32_t right)
{
	r = adapt_compound_rhs(s, idx, left, right, l, r);
	return r == TYPE_ERROR ? TYPE_ERROR : l;
}

TypeId check_compound_assign (Sema *s, uint32_t idx)
{
	uint32_t left = s->ast->nodes[idx].child;
	uint8_t left_kind = s->ast->nodes[left].type;
	uint32_t right = s->ast->nodes[left].next_bro;
	uint8_t op = compound_base[s->ast->nodes[idx].type];

	if (!validate_compound_left(s, idx, left, left_kind, right)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	CompoundLhsResult lr = check_compound_lhs(s, idx, left, left_kind, right);

	if (!check_compound_type_valid(s, idx, lr.l, lr.r)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (type_kind(lr.l) == TK_PTR) {
		TypeId result = check_compound_ptr(s, idx, op, lr.l, lr.r, right);
		s->ast->nodes[idx].data_type = result;
		return result;
	}

	TypeId result = check_compound_regular(s, idx, left, lr.l, lr.r, right);
	s->ast->nodes[idx].data_type = result;
	return result;
}

void check_inc_dec (Sema *s, uint32_t idx)
{
	uint32_t target = s->ast->nodes[idx].child;
	uint8_t target_kind = s->ast->nodes[target].type;

	if (target_kind != NODE_ID && target_kind != NODE_DEREF &&
			target_kind != NODE_INDEX && target_kind != NODE_DOT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operand of '%s' must be a variable",
				s->ast->nodes[idx].type == NODE_INC ? "++" : "--");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}

	if (target_kind == NODE_DOT) {
		ASTNode *dot_left = s->ast->nodes + s->ast->nodes[target].child;
		if (dot_left->type == NODE_ID) {
			uint32_t sym_idx = symtab_find(&s->table, dot_left->str, dot_left->len);
			if (sym_idx != NO_SYMBOL) {
				TypeId ct = s->table.symbols[sym_idx].type;
				if (type_kind(ct) == TK_ARRAY || type_kind(ct) == TK_SLICE) {
					error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
							"operand of '%s' must be a variable",
							s->ast->nodes[idx].type == NODE_INC ? "++" : "--");
					s->ast->nodes[idx].data_type = TYPE_ERROR;
					return;
				}
			}
		}
	}

	TypeId type = check_lvalue(s, target);

	if (target_kind == NODE_DOT && s->ast->nodes[target].type != NODE_DOT) {
		reject_place(s, idx, NO_NODE,
				s->ast->nodes[idx].type == NODE_INC ? "operand of '++'" : "operand of '--'");
		return;
	}
	warn_unassigned(s, target, type);
	mark_read(s, target);
	mark_assigned(s, target);
	set_null_state(s, target, target_kind, 0);
	if (target_kind == NODE_ID) sym_write(s, s->ast->nodes[target].sym, idx);

	if (type != TYPE_ERROR && type_is_aggregate(type)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[type].name);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}
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
