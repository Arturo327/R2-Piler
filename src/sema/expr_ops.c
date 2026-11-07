#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

static int const_operand (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];

	if (n->type == NODE_LIT_i64 || n->type == NODE_LIT_u64 || n->type == NODE_LIT_CHAR)
		return 1;
	if (n->type == NODE_CAST)
		return const_operand(s, n->child);
	return 0;
}

TypeId check_unary (Sema *s, uint32_t idx)
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
	if (type_is_aggregate(operand_type)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator cannot be applied to a value of type %s",
				types[operand_type].name);
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

static void decay_operand (Sema *s, uint32_t *idx, TypeId *type)
{
	TypeKind kind = type_kind(*type);
	TypeId ptr;

	if (kind != TK_ARRAY && kind != TK_SLICE) return;
	ptr = type_ptr_to(type_flat_element(*type));
	if (type_kind(ptr) != TK_PTR) return;

	*idx = sema_wrap_cast(s, *idx, ptr);
	*type = ptr;
}

TypeId check_binary (Sema *s, uint32_t idx)
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
	if (op_type == NODE_ADD || op_type == NODE_SUB) {
		decay_operand(s, &left, &l);
		decay_operand(s, &right, &r);
	}
	if (type_is_aggregate(l) || type_is_aggregate(r)) {
		TypeId bad = type_is_aggregate(l) ? l : r;

		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[bad].name);
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

static TypeId ternary_unify (Sema *s, uint32_t idx, uint32_t *then_idx,
		uint32_t *else_idx, TypeId a, TypeId b)
{
	if (a == TYPE_ERROR || b == TYPE_ERROR) return TYPE_ERROR;

	decay_operand(s, then_idx, &a);
	decay_operand(s, else_idx, &b);
	coerce_string(s, then_idx, &a);
	coerce_string(s, else_idx, &b);
	if (b == TYPE_NULL && type_kind(a) == TK_PTR)
		coerce_null(s, else_idx, &b, a);
	else if (a == TYPE_NULL && type_kind(b) == TK_PTR)
		coerce_null(s, then_idx, &a, b);

	if (a == TYPE_VOID || b == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		return TYPE_ERROR;
	}
	if (type_is_aggregate(a) || type_is_aggregate(b)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[type_is_aggregate(a) ? a : b].name);
		return TYPE_ERROR;
	}
	int a_ptr = type_kind(a) == TK_PTR || a == TYPE_NULL;
	int b_ptr = type_kind(b) == TK_PTR || b == TYPE_NULL;

	if (a != b && a_ptr != b_ptr) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"pointer type mismatch: %s vs %s (use 'as' to convert explicitly)",
				type_display_name(a), type_display_name(b));
		return TYPE_ERROR;
	}
	if (a != b && !unify_operands(s, s->ast->nodes + idx,
			then_idx, else_idx, &a, &b))
		return TYPE_ERROR;
	return a;
}

TypeId check_ternary (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t cond = n->child;
	uint32_t mark = s->table.act_count;
	uint8_t *snap = arena_alloc(s->arena, (size_t)mark * 2);
	uint8_t *acc = snap + mark;

	check_cond(s, cond, 0);
	uint32_t then_idx = s->ast->nodes[cond].next_bro;
	uint32_t else_idx = s->ast->nodes[then_idx].next_bro;

	memset(acc, 1, mark);
	symtab_save_flags(&s->table, snap, mark);
	symtab_clear_pending(&s->table);

	TypeId a = check_expr(s, then_idx);
	symtab_save_flags(&s->table, acc, mark);
	symtab_load_flags(&s->table, snap, mark);
	symtab_clear_pending(&s->table);
	TypeId b = check_expr(s, else_idx);

	symtab_intersect_flags(&s->table, acc, mark);
	symtab_load_flags(&s->table, acc, mark);

	n = s->ast->nodes + idx;
	n->data_type = ternary_unify(s, idx, &then_idx, &else_idx, a, b);
	n = s->ast->nodes + idx;
	n->child = cond;
	s->ast->nodes[cond].next_bro = then_idx;
	s->ast->nodes[then_idx].next_bro = else_idx;
	return n->data_type;
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
	if (type_kind(from) == TK_STRUCT || type_kind(to) == TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast to or from a struct type");
		return 0;
	}
	return 1;
}

TypeId check_cast (Sema *s, uint32_t idx)
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
