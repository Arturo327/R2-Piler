#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

static uint8_t binary_op_id (uint8_t node_type)
{
	switch (node_type)
	{
	case NODE_EQ: return OP_EQ;
	case NODE_NE: return OP_NE;
	case NODE_LT: return OP_LT;
	case NODE_LE: return OP_LE;
	case NODE_GT: return OP_GT;
	case NODE_GE: return OP_GE;
	case NODE_ADD: return OP_ADD;
	case NODE_SUB: return OP_SUB;
	case NODE_MUL: return OP_MUL;
	case NODE_DIV: return OP_DIV;
	case NODE_MOD: return OP_MOD;
	case NODE_AND_A: return OP_AND;
	case NODE_OR_A: return OP_OR;
	case NODE_XOR: return OP_XOR;
	case NODE_LS: return OP_SHL;
	case NODE_RS: return OP_SHR;
	default: return OP_NONE;
	}
}

static uint8_t unary_op_id (uint8_t node_type)
{
	switch (node_type)
	{
	case NODE_NEG: return OP_NEG;
	case NODE_NOT_L: return OP_NOT;
	case NODE_NOT_A: return OP_BNOT;
	default: return OP_NONE;
	}
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

TypeId check_unary (Sema *s, uint32_t idx)
{
	ASTNode *n = &s->ast->nodes[idx];
	uint8_t op = n->type;
	TypeId operand_type = check_expr(s, n->child);

	n = &s->ast->nodes[idx];
	if (operand_type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (op == NODE_NOT_L && type_kind(type_canon(type_unconst_of(operand_type))) == TK_CLASS
			&& class_op_node(s, type_canon(type_unconst_of(operand_type)), OP_NOT) == NO_NODE) {
		operand_type = sema_truth_value(s, n->child, operand_type);
		n = &s->ast->nodes[idx];
		if (operand_type == TYPE_ERROR) {
			n->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}
	int handled = 0;
	TypeId op_result = sema_class_operator(s, idx, n->child, operand_type,
			NO_NODE, unary_op_id(op), &handled);

	if (handled) return op_result;
	if (type_is_aggregate(operand_type)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator cannot be applied to a value of type %s",
				types[operand_type].name);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	if (operand_type == TYPE_VOID || operand_type == TYPE_STR || (op != NODE_NOT_L
				&& (type_ptr_repr(operand_type) != TYPE_ERROR || operand_type == TYPE_NULL))) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator cannot be applied to a value of type %s",
				types[operand_type].name);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = (n->type == NODE_NOT_L) ? TYPE_UNTYPED_INT : type_unconst_of(operand_type);
	int is_lit = const_operand(s, n->child);
	TypeId result_type = type_unconst_of(try_fold_unary(s, idx));

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
		return TYPE_UNTYPED_INT;
	default:
		return type_unconst_of(operand_type);
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

	uint8_t *snap = sema_flags_push(s, mark + 1);
	symtab_save_flags(&s->table, snap, mark);
	sema_clear_pending(s);
	TypeId type = check_expr(s, right);
	symtab_intersect_flags(&s->table, snap, mark);
	sema_load_flags(s, snap, mark);
	sema_flags_pop(s, mark + 1);
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

static int same_pointee (TypeId l, TypeId r)
{
	TypeId lb = types[l].base;
	TypeId rb = types[r].base;

	return lb == rb || type_unconst_of(lb) == type_unconst_of(rb);
}

static TypeId pointer_compare (Sema *s, uint32_t idx, uint32_t *left, uint32_t *right, TypeId *l, TypeId *r)
{
	coerce_string(s, left, l);
	coerce_string(s, right, r);
	TypeId lp = type_ptr_repr(*l);
	TypeId rp = type_ptr_repr(*r);

	if (lp != TYPE_ERROR && rp != TYPE_ERROR) {
		if (!same_pointee(*l, *r)) return pointer_mismatch(s, idx, *l, *r);
		return TYPE_UNTYPED_INT;
	}
	if (lp != TYPE_ERROR && *r == TYPE_NULL) {
		coerce_null(s, right, r, *l);
		return TYPE_UNTYPED_INT;
	}
	if (*l == TYPE_NULL && rp != TYPE_ERROR) {
		coerce_null(s, left, l, *r);
		return TYPE_UNTYPED_INT;
	}
	if (*l == TYPE_NULL && *r == TYPE_NULL) return TYPE_UNTYPED_INT;

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
	TypeId lp = type_ptr_repr(*l);
	TypeId rp = type_ptr_repr(*r);

	if (lp != TYPE_ERROR && is_integer_operand(*r)) {
		if (type_unconst_of(types[lp].base) == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			return TYPE_ERROR;
		}
		*r = finalize_type(s, *right, *r);
		return *l;
	}
	if (op == NODE_ADD && is_integer_operand(*l)
			&& rp != TYPE_ERROR) {
		if (type_unconst_of(types[rp].base) == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			return TYPE_ERROR;
		}
		*l = finalize_type(s, *left, *l);
		return *r;
	}
	if (op == NODE_SUB && lp != TYPE_ERROR
			&& rp != TYPE_ERROR) {
		if (!same_pointee(*l, *r)) return pointer_mismatch(s, idx, *l, *r);
		if (type_unconst_of(types[lp].base) == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"pointer arithmetic on 'ptr void' is not allowed");
			return TYPE_ERROR;
		}
		if (both_str)
			error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
					"subtracting two string literals is probably a mistake;"
					" the result is the byte distance between them");
		return TYPE_UNTYPED_INT;
	}
	if (lp != TYPE_ERROR && rp != TYPE_ERROR) {
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

static int fn_ptr_operator (Sema *s, uint32_t idx, uint8_t op)
{
	ASTNode *n = s->ast->nodes + idx;
	const char *msg;

	if (op == NODE_ADD || op == NODE_SUB)
		msg = "pointer arithmetic on a function pointer is not allowed";
	else msg = "comparison of function pointers is not allowed; only '==' and '!=' are supported";
	error_report(s->err, ERR_ERROR, node_loc(n), "%s", msg);
	n->data_type = TYPE_ERROR;
	return 1;
}

static int check_pointer_binary (Sema *s, uint32_t idx, uint32_t *left,
		uint32_t *right, TypeId *l, TypeId *r)
{
	uint8_t op = s->ast->nodes[idx].type;
	TypeId result;

	if (!is_pointer_operand(*l) && !is_pointer_operand(*r)) return 0;
	if (op == NODE_AND_L || op == NODE_OR_L) return 0;

	TypeId lp = type_ptr_repr(*l);
	TypeId rp = type_ptr_repr(*r);
	int l_fn_ptr = lp != TYPE_ERROR && type_kind(types[lp].base) == TK_FN;
	int r_fn_ptr = rp != TYPE_ERROR && type_kind(types[rp].base) == TK_FN;
	if ((l_fn_ptr || r_fn_ptr) && op != NODE_EQ && op != NODE_NE)
		return fn_ptr_operator(s, idx, op);

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

	agg_untrack_root(s, *idx);
	*idx = sema_wrap_cast(s, *idx, ptr);
	*type = ptr;
}

static int same_expr (Sema *s, uint32_t a, uint32_t b);

static int same_children (Sema *s, uint32_t a, uint32_t b)
{
	while (a != NO_NODE && b != NO_NODE) {
		if (!same_expr(s, a, b)) return 0;
		a = s->ast->nodes[a].next_bro;
		b = s->ast->nodes[b].next_bro;
	}
	return a == b;
}

static int same_expr (Sema *s, uint32_t a, uint32_t b)
{
	ASTNode *na = s->ast->nodes + a;
	ASTNode *nb = s->ast->nodes + b;

	if (na->type != nb->type) return 0;

	switch (na->type)
	{
	case NODE_LIT_STR: case NODE_TERNARY: return 0;
	case NODE_LIT_NULL: return 1;
	case NODE_LIT_i64: return na->i64 == nb->i64;
	case NODE_LIT_u64: return na->u64 == nb->u64;
	case NODE_LIT_CHAR: return na->chr == nb->chr;
	case NODE_ID:
		if (na->sym != NO_SYMBOL || nb->sym != NO_SYMBOL)
			return na->sym != NO_SYMBOL && na->sym == nb->sym;
		return na->len == nb->len && memcmp(na->str, nb->str, na->len) == 0;
	case NODE_DOT: case NODE_FIELD_INIT:
		if (na->len != nb->len || memcmp(na->str, nb->str, na->len) != 0)
			return 0;
		break;
	default: break;
	}

	if (na->data_type != nb->data_type) return 0;
	return same_children(s, na->child, nb->child);
}

static int const_compare (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->type == NODE_LIT_NULL) return 1;
	return const_operand(s, idx);
}

static void warn_self_compare (Sema *s, uint32_t idx, uint32_t left, uint32_t right)
{
	const char *result;

	switch (s->ast->nodes[idx].type)
	{
	case NODE_EQ: case NODE_GE: case NODE_LE: result = "true"; break;
	case NODE_NE: case NODE_GT: case NODE_LT: result = "false"; break;
	default: return;
	}
	if (expr_has_effect(s, left) || expr_has_effect(s, right)) return;
	if (const_compare(s, left) && const_compare(s, right)) return;
	if (!same_expr(s, left, right)) return;

	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"comparison of an expression with itself is always %s;"
			" did you mean to compare two different values?", result);
}

static void warn_shift_count (Sema *s, uint32_t idx, TypeId type, uint32_t count)
{
	TypeKind kind = type_kind(type);
	TypeId count_type = s->ast->nodes[count].data_type;

	if (kind != TK_INT && kind != TK_ENUM && kind != TK_UNTYPED_INT) return;
	uint64_t bits;
	if (!lit_node_bits(s, s->ast->nodes + count, &bits)) return;

	if (types[count_type].sign && (int64_t)bits < 0) {
		error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + count),
				"negative shift count");
		return;
	}
	uint32_t width = types[type].size * 8u;
	if (bits < width) return;
	if (kind == TK_UNTYPED_INT)
		error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
				"shift count %llu is out of range for a %u-bit value",
				(unsigned long long)bits, width);
	else error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"shift count %llu is out of range for type %s (%u bits)",
			(unsigned long long)bits, types[type].name, width);
}

static int zero_literal (Sema *s, uint32_t idx)
{
	uint64_t bits;

	return lit_node_bits(s, s->ast->nodes + idx, &bits) && bits == 0;
}

static void warn_unsigned_zero (Sema *s, uint32_t idx, uint32_t left, uint32_t right, TypeId type)
{
	uint8_t op = s->ast->nodes[idx].type;
	TypeKind kind = type_kind(type);

	if (kind != TK_INT && kind != TK_ENUM && kind != TK_NEWTYPE) return;
	if (types[type].sign) return;
	if (op != NODE_LT && op != NODE_LE && op != NODE_GT && op != NODE_GE) return;

	int zero_left = zero_literal(s, left);
	int zero_right = zero_literal(s, right);
	if (zero_left == zero_right) return;

	int always_true;
	if (zero_right) {
		if (op == NODE_GE) always_true = 1;
		else if (op == NODE_LT) always_true = 0;
		else return;
	} else {
		if (op == NODE_LE) always_true = 1;
		else if (op == NODE_GT) always_true = 0;
		else return;
	}

	error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + idx),
			"comparison of an unsigned value with 0 is always %s",
			always_true ? "true" : "false");
}

static int binary_operands_bad (Sema *s, uint32_t idx, TypeId l, TypeId r)
{
	if (type_is_aggregate(l) || type_is_aggregate(r)) {
		TypeId bad = type_is_aggregate(l) ? l : r;

		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[bad].name);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return 1;
	}
	if (l == TYPE_VOID || r == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return 1;
	}
	return 0;
}

TypeId check_binary (Sema *s, uint32_t idx)
{
	uint8_t op_type = s->ast->nodes[idx].type;
	uint32_t left = s->ast->nodes[idx].child;
	uint32_t right = s->ast->nodes[left].next_bro;
	TypeId l = check_expr(s, left);
	int handled = 0;
	TypeId op_result = sema_class_operator(s, idx, left, l, right,
			binary_op_id(op_type), &handled);

	if (handled) return op_result;
	if (l == TYPE_ERROR) {
		check_rhs(s, op_type, right);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId r = check_rhs(s, op_type, right);

	if (r == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId reversed = sema_class_reverse(s, idx, right, r, left, l,
			binary_op_id(op_type), &handled);

	if (handled) return reversed;
	if (op_type == NODE_AND_L || op_type == NODE_OR_L) {
		l = sema_truth_value(s, left, l);
		r = sema_truth_value(s, right, r);
		if (l == TYPE_ERROR || r == TYPE_ERROR) {
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}
	warn_self_compare(s, idx, left, right);
	if (op_type == NODE_ADD || op_type == NODE_SUB) {
		decay_operand(s, &left, &l);
		decay_operand(s, &right, &r);
	}
	if (binary_operands_bad(s, idx, l, r)) return TYPE_ERROR;

	if (op_type != NODE_AND_L && op_type != NODE_OR_L
			&& check_pointer_binary(s, idx, &left, &right, &l, &r))
		return s->ast->nodes[idx].data_type;

	if (op_type == NODE_AND_L || op_type == NODE_OR_L) {
		if (!check_logic(s, idx, left, right, &l, &r)) return TYPE_ERROR;
	} else if (op_type == NODE_LS || op_type == NODE_RS) {
		finalize_type(s, right, r);
		if (!is_literal_node(s, right))
			l = finalize_type(s, left, l);
		warn_shift_count(s, idx, l, right);
	}

	if (l != r && needs_common_type(op_type)
			&& !unify_operands(s, s->ast->nodes + idx, &left, &right, &l, &r)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (l == r) warn_unsigned_zero(s, idx, left, right, l);

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
	if (b == TYPE_NULL && type_ptr_repr(a) != TYPE_ERROR)
		coerce_null(s, else_idx, &b, a);
	else if (a == TYPE_NULL && type_ptr_repr(b) != TYPE_ERROR)
		coerce_null(s, then_idx, &a, b);

	if (a == TYPE_VOID || b == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type void");
		return TYPE_ERROR;
	}
	if ((type_is_aggregate(a) || type_is_aggregate(b))
			&& !(type_kind(a) == TK_CLASS && a == b)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"operator cannot be applied to a value of type %s",
				types[type_is_aggregate(a) ? a : b].name);
		return TYPE_ERROR;
	}
	int a_ptr = type_ptr_repr(a) != TYPE_ERROR || a == TYPE_NULL;
	int b_ptr = type_ptr_repr(b) != TYPE_ERROR || b == TYPE_NULL;

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
	uint8_t *snap = sema_flags_push(s, (size_t)mark * 2);
	uint8_t *acc = snap + mark;

	check_cond(s, cond, 0);
	uint32_t then_idx = s->ast->nodes[cond].next_bro;
	uint32_t else_idx = s->ast->nodes[then_idx].next_bro;

	memset(acc, 1, mark);
	symtab_save_flags(&s->table, snap, mark);
	sema_clear_pending(s);

	TypeId a = check_expr(s, then_idx);
	symtab_save_flags(&s->table, acc, mark);
	sema_load_flags(s, snap, mark);
	sema_clear_pending(s);
	TypeId b = check_expr(s, else_idx);

	symtab_intersect_flags(&s->table, acc, mark);
	sema_load_flags(s, acc, mark);

	n = s->ast->nodes + idx;
	n->data_type = ternary_unify(s, idx, &then_idx, &else_idx, a, b);
	n = s->ast->nodes + idx;
	n->child = cond;
	s->ast->nodes[cond].next_bro = then_idx;
	s->ast->nodes[then_idx].next_bro = else_idx;
	sema_flags_pop(s, (size_t)mark * 2);
	return n->data_type;
}

static int cast_types_ok (Sema *s, ASTNode *n, TypeId from, TypeId to)
{
	if (from == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast a value of type void");
		return 0;
	}
	if ((to == TYPE_ANY && (from == TYPE_ANY || sema_any_packable(from)))
			|| (from == TYPE_ANY && sema_any_packable(to)))
		return 1;
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
		const char *via = type_ptr_repr(from) != TYPE_ERROR
				? "pointer type" : "integer type";

		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast between newtype types '%s' and '%s';"
				" cast through the underlying %s",
				types[from].name, types[to].name, via);
		return 0;
	}
	if (type_is_struct_like(from) || type_is_struct_like(to)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot cast to or from a struct type");
		return 0;
	}
	return 1;
}

static TypeId check_cast_literal (Sema *s, uint32_t idx, TypeId to)
{
	uint8_t child_type = s->ast->nodes[s->ast->nodes[idx].child].type;
	ASTNode *n = s->ast->nodes + idx;

	if (child_type != NODE_STRUCT_INIT && child_type != NODE_ARRAY_INIT)
		return TYPE_VOID;
	if (!type_is_struct_like(to)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a '{...}' literal can only be cast to a struct or union type");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	uint32_t init = n->child;

	if (!as_struct_init(s, init, to)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	check_struct_init(s, init, to);
	n = s->ast->nodes + idx;
	n->flags |= NODE_FLAG_LITERAL;
	n->data_type = to;
	return to;
}

TypeId check_cast (Sema *s, uint32_t idx)
{
	TypeId to = resolve_type_node(s, s->ast->nodes + idx);
	ASTNode *n = s->ast->nodes + idx;

	if (to == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId literal = check_cast_literal(s, idx, to);

	if (literal != TYPE_VOID) return literal;
	n = s->ast->nodes + idx;
	TypeId from = check_expr(s, n->child);

	n = s->ast->nodes + idx;
	if (from == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (!cast_types_ok(s, n, from, to)) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(from) == TK_ARRAY || type_kind(from) == TK_SLICE)
		agg_untrack_root(s, n->child);
	if (from == to)
		error_report(s->err, ERR_WARNING, node_loc(n),
				"redundant cast to '%s'; the value already has that type",
				types[to].name);

	finalize_type(s, n->child, from);
	return n->data_type;
}
