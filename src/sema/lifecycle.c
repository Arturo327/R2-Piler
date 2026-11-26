#include "sema/common.h"

static uint32_t target_root (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	while (n->type == NODE_CAST && n->child != NO_NODE)
		n = s->ast->nodes + n->child;
	if (n->type == NODE_DEREF && (n->flags & NODE_FLAG_REF)
			&& n->child != NO_NODE)
		n = s->ast->nodes + n->child;
	if (n->type != NODE_ID || n->sym == NO_SYMBOL) return NO_SYMBOL;
	return n->sym;
}

int sema_borrowed_target (Sema *s, uint32_t target, const char *verb)
{
	uint32_t sym = target_root(s, target);

	if (sym == NO_SYMBOL || s->table.symbols[sym].kind != SYMBOL_PARAM) return 0;
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + target),
			"cannot '%s' a parameter; parameters are borrowed", verb);
	return 1;
}

static TypeId target_class (TypeId type, int *is_ptr)
{
	TypeId value = type_canon(type_unconst_of(type));

	if (type_kind(value) == TK_CLASS) {
		*is_ptr = 0;
		return value;
	}
	TypeId ptr = type_ptr_repr(type);

	if (ptr == TYPE_ERROR) return TYPE_ERROR;
	value = type_canon(type_unconst_of(types[ptr].base));
	if (type_kind(value) != TK_CLASS) return TYPE_ERROR;
	*is_ptr = 1;
	return value;
}

static int target_const (TypeId type)
{
	TypeId repr = type_repr(type);

	if (type_is_const(type) || type_is_const(repr)) return 1;
	TypeId ptr = type_ptr_repr(type);

	return ptr != TYPE_ERROR && type_is_const(types[ptr].base);
}

static int is_lvalue (Sema *s, uint32_t idx)
{
	int type = s->ast->nodes[idx].type;

	return type == NODE_ID || type == NODE_DOT || type == NODE_INDEX || type == NODE_DEREF;
}

static uint32_t address_value (Sema *s, uint32_t value, TypeId cls)
{
	uint32_t root = target_root(s, value);
	uint32_t addr = sema_wrap_addr(s, value);

	s->ast->nodes[addr].data_type = type_ptr_to(cls);
	if (root != NO_SYMBOL) s->table.symbols[root].addressed = 1;
	agg_untrack_root(s, value);
	return addr;
}

static int drop_target (Sema *s, uint32_t idx, uint32_t *target, TypeId *cls)
{
	uint32_t t = s->ast->nodes[idx].child;
	TypeId type = check_expr(s, t);
	int is_ptr = 0;

	if (type == TYPE_ERROR) return 0;
	if (sema_borrowed_target(s, t, "drop")) return 0;
	if (target_const(type)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"the target of 'drop' cannot be const");
		return 0;
	}
	*cls = target_class(type, &is_ptr);
	if (*cls == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"'drop' requires a class or a pointer to a class, not %s",
				type_display_name(type));
		return 0;
	}
	if (!is_ptr) {
		if (!is_lvalue(s, t)) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + t),
					"the target of 'drop' must be a variable, a field, an index or a dereference");
			return 0;
		}
		t = address_value(s, t, *cls);
		s->ast->nodes[idx].child = t;
	}
	*target = t;
	return 1;
}

static int reset_target (Sema *s, uint32_t idx, uint32_t *target, TypeId *cls)
{
	uint32_t t = (uint32_t)s->ast->nodes[idx].u64;
	TypeId type = check_expr(s, t);
	int is_ptr = 0;

	if (type == TYPE_ERROR) return 0;
	if (!is_lvalue(s, t)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + t),
				"the target of 'reset' must be a variable, a field, an index or a dereference");
		return 0;
	}
	if (target_const(type)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"the target of 'reset' cannot be const");
		return 0;
	}
	*cls = target_class(type, &is_ptr);
	if (*cls == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"'reset' requires a class or a pointer to a class, not %s",
				type_display_name(type));
		return 0;
	}
	*target = is_ptr ? t : address_value(s, t, *cls);
	return 1;
}

void check_drop (Sema *s, uint32_t idx)
{
	uint32_t target;
	TypeId cls;

	if (!drop_target(s, idx, &target, &cls)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}
	uint32_t destroy = class_destroy_node(s, cls);

	if (destroy == NO_NODE) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"class '%s' has no destructor; 'drop' is not available",
				types[cls].name);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}
	s->ast->nodes[idx].child = target;
	s->ast->nodes[idx].sym = s->ast->nodes[destroy].sym;
	s->ast->nodes[idx].data_type = TYPE_VOID;
}

static uint32_t check_reset_args (Sema *s, uint32_t idx, TypeId cls,
		uint32_t init, uint32_t args)
{
	if (init == NO_NODE) {
		for (uint32_t a = args; a != NO_NODE; a = s->ast->nodes[a].next_bro)
			check_expr(s, a);
		return args;
	}
	ASTNode call = {0};

	call.type = NODE_FN_CALL;
	call.child = args;
	call.str = (char *)types[cls].name;
	call.len = types[cls].len;
	call.sym = NO_SYMBOL;
	call.type_node = NO_TYPE_NODE;
	call.line = s->ast->nodes[idx].line;
	call.col = s->ast->nodes[idx].col;

	uint32_t call_idx = sema_new_node(s, call);

	check_ctor_args(s, call_idx, init);
	return s->ast->nodes[call_idx].child;
}

void check_reset (Sema *s, uint32_t idx)
{
	uint32_t target;
	TypeId cls;
	uint32_t args = s->ast->nodes[idx].child;

	if (!reset_target(s, idx, &target, &cls)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}
	uint32_t destroy = class_destroy_node(s, cls);
	uint32_t init = class_init_node(s, cls);

	if (destroy == NO_NODE)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"class '%s' has no destructor; 'reset' is not available",
				types[cls].name);
	if (init == NO_NODE)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"class '%s' has no initializer; 'reset' is not available",
				types[cls].name);

	args = check_reset_args(s, idx, cls, init, args);
	if (destroy == NO_NODE || init == NO_NODE) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return;
	}
	s->ast->nodes[target].next_bro = args;
	s->ast->nodes[idx].child = target;
	s->ast->nodes[idx].u64 = s->ast->nodes[init].sym;
	s->ast->nodes[idx].sym = s->ast->nodes[destroy].sym;
	s->ast->nodes[idx].data_type = TYPE_VOID;
}
