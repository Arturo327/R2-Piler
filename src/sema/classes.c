#include "sema/common.h"
#include "sema/expr_internal.h"

#include <stdio.h>
#include <string.h>

static uint32_t class_decl_node (Sema *s, TypeId cls)
{
	if (type_kind(cls) != TK_CLASS) return NO_NODE;

	uint32_t decl = types[cls].decl;

	if (decl >= s->ast->count || s->ast->nodes[decl].type != NODE_CLASS_DEC)
		return NO_NODE;
	return decl;
}

uint32_t class_fn_has_self (Sema *s, uint32_t fn_idx)
{
	uint32_t args = s->ast->nodes[fn_idx].child;
	uint32_t first = s->ast->nodes[args].child;

	return first != NO_NODE && (s->ast->nodes[first].flags & NODE_FLAG_SELF);
}

uint32_t class_method_node (Sema *s, TypeId cls, const char *name, uint16_t len)
{
	uint32_t decl = class_decl_node(s, cls);

	if (decl == NO_NODE) return NO_NODE;
	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type != NODE_FN_DEC || !(fn->flags & NODE_FLAG_METHOD)) continue;
		if (fn->op != OP_NONE) continue;
		if (fn->sym == NO_SYMBOL) continue;
		if (fn->len == len && memcmp(fn->str, name, len) == 0) return m;
	}
	return NO_NODE;
}

uint32_t class_op_node (Sema *s, TypeId cls, uint8_t op)
{
	uint32_t decl = class_decl_node(s, cls);

	if (decl == NO_NODE) return NO_NODE;
	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type != NODE_FN_DEC || fn->op != op) continue;
		if (fn->sym != NO_SYMBOL) return m;
	}
	return NO_NODE;
}

uint32_t class_init_node (Sema *s, TypeId cls)
{
	uint32_t decl = class_decl_node(s, cls);

	if (decl == NO_NODE) return NO_NODE;
	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type == NODE_FN_DEC && (fn->flags & NODE_FLAG_INIT)
				&& fn->sym != NO_SYMBOL)
			return m;
	}
	return NO_NODE;
}

uint32_t class_destroy_node (Sema *s, TypeId cls)
{
	uint32_t decl = class_decl_node(s, cls);

	if (decl == NO_NODE) return NO_NODE;
	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type == NODE_FN_DEC && (fn->flags & NODE_FLAG_DESTROY)
				&& fn->sym != NO_SYMBOL)
			return m;
	}
	return NO_NODE;
}

int reject_special_call (Sema *s, uint32_t use, uint32_t sym)
{
	if (sym == NO_SYMBOL) return 0;

	Symbol *fn = s->table.symbols + sym;

	if (fn->kind != SYMBOL_FN || fn->decl == NO_NODE) return 0;
	uint16_t flags = s->ast->nodes[fn->decl].flags;

	if (flags & NODE_FLAG_INIT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
				"the initializer of class '%s' cannot be called directly;"
				" use '%s(...)'", types[fn->owner].name, types[fn->owner].name);
	} else if (flags & NODE_FLAG_DESTROY) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
				"the destructor of class '%s' cannot be called directly;"
				" use 'drop', 'delete' or 'reset'", types[fn->owner].name);
	} else return 0;
	s->ast->nodes[use].data_type = TYPE_ERROR;
	return 1;
}

void check_method_visibility (Sema *s, uint32_t sym, uint32_t use)
{
	Symbol *fn = s->table.symbols + sym;
	ASTNode *decl = s->ast->nodes + fn->decl;

	if (!(decl->flags & NODE_FLAG_METHOD)) return;
	if (!(decl->flags & NODE_FLAG_PRIV)) return;
	if (s->curr_class == fn->owner) return;
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
			"method '%.*s::%.*s' is private", (int)types[fn->owner].len,
			types[fn->owner].name, (int)decl->len, decl->str);
}

static void mark_class_self (Sema *s, uint32_t fn_idx, TypeId owner)
{
	uint32_t args = s->ast->nodes[fn_idx].child;
	uint32_t p = s->ast->nodes[args].child;
	int first = 1;

	s->ast->nodes[fn_idx].flags |= NODE_FLAG_METHOD;
	while (p != NO_NODE) {
		ASTNode *param = s->ast->nodes + p;

		if (param->flags & NODE_FLAG_SELF) {
			if (!first) error_report(s->err, ERR_ERROR, node_loc(param),
					"'self' must be the first parameter of a method");
			else if (param->child != NO_NODE)
				error_report(s->err, ERR_ERROR, node_loc(param),
						"'self' cannot have a default value");
			if (first) {
				TypeId ptr = type_ptr_to(owner);

				param->type_node = NO_TYPE_NODE;
				param->data_type = ptr;
				if (ptr == TYPE_ERROR)
					error_report(s->err, ERR_ERROR, node_loc(param),
							"too many types");
			}
		}
		first = 0;
		p = s->ast->nodes[p].next_bro;
	}
}

static inline int fn_named (ASTNode *fn, const char *name, uint16_t len)
{
	return fn->len == len && memcmp(fn->str, name, len) == 0;
}

static int reject_normal_special (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	Type *ct = types + owner;

	if (fn_named(fn, "init", 4)) {
		if (fn->flags & NODE_FLAG_PRIV)
			error_report(s->err, ERR_ERROR, node_loc(fn),
					"an initializer cannot be private");
		else error_report(s->err, ERR_ERROR, node_loc(fn),
					"the initializer of class '%s' must be declared as"
					" 'init (self, ...)' without 'fn'", ct->name);
	} else if (fn_named(fn, "destroy", 7)) {
		if (fn->flags & NODE_FLAG_PRIV)
			error_report(s->err, ERR_ERROR, node_loc(fn),
					"a destructor cannot be private");
		else error_report(s->err, ERR_ERROR, node_loc(fn),
					"the destructor of class '%s' must be declared as"
					" 'destroy (self) { ... }' without 'fn'", ct->name);
	}
	else if (fn->len == ct->len && memcmp(fn->str, ct->name, ct->len) == 0)
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"a method cannot have the same name as its class");
	else return 0;
	return 1;
}

static int class_has_init_before (Sema *s, TypeId owner, uint32_t fn_idx)
{
	uint32_t decl = class_decl_node(s, owner);

	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE && m != fn_idx;
			m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type == NODE_FN_DEC && (fn->flags & NODE_FLAG_INIT)) return 1;
	}
	return 0;
}

static int class_has_destroy_before (Sema *s, TypeId owner, uint32_t fn_idx)
{
	uint32_t decl = class_decl_node(s, owner);

	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE && m != fn_idx;
			m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type == NODE_FN_DEC && (fn->flags & NODE_FLAG_DESTROY)) return 1;
	}
	return 0;
}

static int destroy_params_ok (Sema *s, uint32_t fn_idx)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	uint32_t args = fn->child;
	uint32_t first = s->ast->nodes[args].child;

	if (first == NO_NODE || !(s->ast->nodes[first].flags & NODE_FLAG_SELF)) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"a destructor must take 'self' as its first parameter");
		return 0;
	}
	if (s->ast->nodes[first].next_bro != NO_NODE) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"a destructor cannot take parameters other than 'self'");
		return 0;
	}
	if (s->ast->nodes[args].flags & (NODE_FLAG_VARARG | NODE_FLAG_VARARG_TYPED)) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"a destructor cannot be variadic");
		return 0;
	}
	return 1;
}

static void register_class_method (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	Type *ct = types + owner;
	uint32_t ret = s->ast->nodes[fn->child].next_bro;
	size_t cap = (size_t)ct->len + (size_t)fn->len + 3;
	char *qname = arena_alloc(s->arena, cap);
	int qlen = snprintf(qname, cap, "%.*s::%.*s", (int)ct->len, ct->name,
			(int)fn->len, fn->str);

	if (qlen < 0 || qlen > UINT16_MAX) {
		error_report(s->err, ERR_ERROR, node_loc(fn), "method name is too long");
		return;
	}
	uint32_t sym = sema_declare(s, qname, (uint16_t)qlen, SYMBOL_FN,
			s->ast->nodes[ret].data_type, fn_idx);
	Symbol *method = s->table.symbols + sym;

	method->owner = owner;
	method->link = sema_import_link(s, fn_idx);
	method->fn_type = build_fn_value(s, fn_idx);
	if (find_field(owner, fn->str, fn->len))
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"class '%s' already has a field named '%.*s'",
				ct->name, (int)fn->len, fn->str);
}

static uint32_t fn_param_count (Sema *s, uint32_t fn_idx)
{
	uint32_t count = 0;
	uint32_t args = s->ast->nodes[fn_idx].child;

	for (uint32_t p = s->ast->nodes[args].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro)
		count++;
	return count;
}

static int class_has_op_before (Sema *s, TypeId owner, uint32_t fn_idx, uint8_t op)
{
	uint32_t decl = class_decl_node(s, owner);

	for (uint32_t m = s->ast->nodes[decl].child; m != NO_NODE && m != fn_idx;
			m = s->ast->nodes[m].next_bro) {
		ASTNode *fn = s->ast->nodes + m;

		if (fn->type == NODE_FN_DEC && fn->op == op) return 1;
	}
	return 0;
}

static inline int unary_canon (uint8_t op)
{
	return op == OP_NEG || op == OP_NOT || op == OP_BNOT
			|| op == OP_INC || op == OP_DEC;
}

static int op_params_ok (Sema *s, uint32_t fn_idx)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	uint32_t args = fn->child;
	uint32_t count = fn_param_count(s, fn_idx);
	uint32_t want = unary_canon(fn->op) ? 1 : 2;
	int ok = 1;

	if (fn->op == OP_INDEX && count < 2) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"operator '[]' needs at least one index parameter");
		ok = 0;
	} else if (fn->op != OP_INDEX && count != want) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				want == 1 ? "unary operator '%s' takes only 'self'"
				: "operator '%s' takes 'self' and one operand",
				op_spelling(fn->op));
		ok = 0;
	}

	if (s->ast->nodes[args].flags & (NODE_FLAG_VARARG | NODE_FLAG_VARARG_TYPED)) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"an operator cannot be variadic");
		ok = 0;
	}
	uint32_t p = s->ast->nodes[args].child;

	if (p != NO_NODE) p = s->ast->nodes[p].next_bro;
	for (; p != NO_NODE; p = s->ast->nodes[p].next_bro) {
		if (s->ast->nodes[p].child != NO_NODE) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"operator parameters cannot have default values");
			ok = 0;
			break;
		}
	}
	return ok;
}

static int op_index_types_ok (Sema *s, uint32_t fn_idx)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	uint32_t args = fn->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	TypeId rtype = s->ast->nodes[ret].data_type;
	int ok = 1;

	if (type_ptr_repr(rtype) == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"operator '[]' must return a pointer type");
		return 0;
	}
	for (uint32_t p = s->ast->nodes[args].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro) {
		TypeId pt = s->ast->nodes[p].data_type;
		TypeKind kind = type_kind(type_canon(type_unconst_of(pt)));

		if (pt == TYPE_ERROR || (s->ast->nodes[p].flags & NODE_FLAG_SELF))
			continue;
		if (kind != TK_INT && kind != TK_ENUM) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"index parameter '%.*s' of operator '[]'"
					" must be an integer type",
					(int)s->ast->nodes[p].len, s->ast->nodes[p].str);
			ok = 0;
		}
	}
	return ok;
}

static int op_assign_types_ok (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	uint32_t first = s->ast->nodes[s->ast->nodes[fn->child].child].next_bro;
	TypeId operand = s->ast->nodes[first].data_type;

	if (operand == TYPE_ERROR || type_canon(type_unconst_of(operand)) == owner)
		return 1;
	error_report(s->err, ERR_ERROR, node_loc(fn),
			"operator '=' of class '%s' must take a value of the class by value",
			types[owner].name);
	return 0;
}

static int op_types_ok (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	uint32_t args = fn->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	TypeId rtype = s->ast->nodes[ret].data_type;
	int ok = 1;

	if (rtype == TYPE_ERROR) return 0;
	if (s->ast->nodes[ret].child != NO_NODE) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"an operator cannot return several values");
		return 0;
	}
	if ((fn->op == OP_NOT || (fn->op >= OP_EQ && fn->op <= OP_GE))
			&& type_canon(type_unconst_of(rtype)) != TYPE_i64) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"operator '%s' must return i64", op_spelling(fn->op));
		ok = 0;
	}
	if (fn->op == OP_ASSIGN) ok = op_assign_types_ok(s, fn_idx, owner) && ok;
	if (fn->op != OP_INDEX) return ok;
	return op_index_types_ok(s, fn_idx) && ok;
}

static void register_op_method (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	Type *ct = types + owner;
	const char *canon = op_canon(fn->op);
	size_t cap = (size_t)ct->len + strlen(canon) + 16;
	char *qname = arena_alloc(s->arena, cap);
	int qlen = snprintf(qname, cap, "%.*s::op$%s", (int)ct->len, ct->name, canon);
	uint32_t ret = s->ast->nodes[fn->child].next_bro;

	if (qlen < 0 || qlen > UINT16_MAX) {
		error_report(s->err, ERR_ERROR, node_loc(fn), "operator name is too long");
		return;
	}
	uint32_t sym = sema_declare(s, qname, (uint16_t)qlen, SYMBOL_FN,
			s->ast->nodes[ret].data_type, fn_idx);
	Symbol *method = s->table.symbols + sym;

	method->owner = owner;
	method->link = sema_import_link(s, fn_idx);
	method->fn_type = build_fn_value(s, fn_idx);
}

static void declare_class_op (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;

	if (class_has_op_before(s, owner, fn_idx, fn->op)) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"class '%s' already has operator '%s'", types[owner].name,
				op_spelling(fn->op));
		return;
	}
	mark_class_self(s, fn_idx, owner);
	if (!class_fn_has_self(s, fn_idx)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + fn_idx),
				"an operator must take 'self' as its first parameter");
		return;
	}
	if (!op_params_ok(s, fn_idx)) return;
	resolve_fn_types(s, fn_idx);
	if (!op_types_ok(s, fn_idx, owner)) return;
	register_op_method(s, fn_idx, owner);
}

static void declare_class_method (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	int is_init = (fn->flags & NODE_FLAG_INIT) != 0;
	int is_destroy = (fn->flags & NODE_FLAG_DESTROY) != 0;

	if (fn->op != OP_NONE) {
		declare_class_op(s, fn_idx, owner);
		return;
	}
	if (!is_init && !is_destroy && reject_normal_special(s, fn_idx, owner)) return;
	if ((is_init || is_destroy) && (fn->flags & NODE_FLAG_PRIV))
		error_report(s->err, ERR_ERROR, node_loc(fn),
				is_init ? "an initializer cannot be private"
				: "a destructor cannot be private");
	if (is_init && class_has_init_before(s, owner, fn_idx)) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"class '%s' already has an initializer", types[owner].name);
		return;
	}
	if (is_destroy && class_has_destroy_before(s, owner, fn_idx)) {
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"class '%s' already has a destructor", types[owner].name);
		return;
	}

	mark_class_self(s, fn_idx, owner);
	if (is_init && !class_fn_has_self(s, fn_idx))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + fn_idx),
				"an initializer must take 'self' as its first parameter");
	if (is_destroy && !destroy_params_ok(s, fn_idx)) return;
	resolve_fn_types(s, fn_idx);
	register_class_method(s, fn_idx, owner);
}

void decl_class_methods (Sema *s, uint32_t idx)
{
	ASTNode *cls = s->ast->nodes + idx;

	if (cls->sym == NO_SYMBOL) return;
	Symbol *csym = s->table.symbols + cls->sym;

	if (csym->type == TYPE_ERROR || type_kind(csym->type) != TK_CLASS) return;
	for (uint32_t m = cls->child; m != NO_NODE; m = s->ast->nodes[m].next_bro)
		if (s->ast->nodes[m].type == NODE_FN_DEC)
			declare_class_method(s, m, csym->type);
}

void check_class_methods (Sema *s, uint32_t idx)
{
	ASTNode *cls = s->ast->nodes + idx;

	if (cls->sym == NO_SYMBOL) return;
	Symbol *csym = s->table.symbols + cls->sym;

	if (csym->type == TYPE_ERROR || type_kind(csym->type) != TK_CLASS) return;
	TypeId saved = s->curr_class;

	s->curr_class = csym->type;
	uint32_t m = cls->child;
	while (m != NO_NODE) {
		uint32_t next = s->ast->nodes[m].next_bro;

		if (s->ast->nodes[m].type == NODE_FN_DEC
				&& s->ast->nodes[m].sym != NO_SYMBOL)
			check_fn_dec(s, m);
		m = next;
	}
	s->curr_class = saved;
}

static TypeId receiver_class (TypeId type)
{
	TypeId value = type_unconst_of(type);

	if (type_kind(value) == TK_CLASS) return value;
	TypeId repr = type_ptr_repr(value);

	if (repr == TYPE_ERROR) return TYPE_ERROR;
	value = type_unconst_of(types[repr].base);
	return type_kind(value) == TK_CLASS ? value : TYPE_ERROR;
}

static uint32_t make_receiver (Sema *s, uint32_t child, TypeId ptr)
{
	ASTNode *c = s->ast->nodes + child;
	ASTNode raw = {0};

	raw.type = NODE_ADDR;
	raw.flags = NODE_FLAG_TEMP_ADDR;
	raw.data_type = ptr;
	raw.line = c->line;
	raw.col = c->col;
	raw.len = 1;
	raw.child = child;
	raw.next_bro = NO_NODE;
	raw.sym = NO_SYMBOL;
	raw.type_node = NO_TYPE_NODE;
	return sema_new_node(s, raw);
}

static TypeId finish_method_call (Sema *s, uint32_t idx, uint32_t sym, uint32_t receiver, uint32_t args)
{
	ASTNode *call = s->ast->nodes + idx;
	Symbol *fn = s->table.symbols + sym;

	call->flags &= ~NODE_FLAG_INDIRECT;
	call->sym = sym;
	if (receiver != NO_NODE) {
		s->ast->nodes[receiver].next_bro = args;
		call->child = receiver;
	} else call->child = args;
	if (sym != s->curr_fn) fn->read = 1;
	check_call_args(s, idx, fn->decl);
	call = s->ast->nodes + idx;
	call->data_type = type_unconst_of(fn->type);
	return call->data_type;
}

TypeId sema_op_call (Sema *s, uint32_t idx, uint32_t receiver, TypeId cls,
		uint32_t args, uint32_t method)
{
	uint32_t sym = s->ast->nodes[method].sym;
	TypeId ptr = type_ptr_to(cls);

	if (ptr == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + method),
				"too many types");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	uint32_t recv = make_receiver(s, receiver, ptr);
	const char *spelling = op_spelling(s->ast->nodes[method].op);

	s->ast->nodes[idx].type = NODE_FN_CALL;
	s->ast->nodes[idx].str = (char *)spelling;
	s->ast->nodes[idx].len = (uint16_t)strlen(spelling);
	s->ast->nodes[idx].op = 0;
	TypeId type = finish_method_call(s, idx, sym, recv, args);

	if (s->ast->nodes[method].op == OP_INDEX)
		s->ast->nodes[idx].op = OP_INDEX;
	return type;
}

static TypeId class_of (TypeId type)
{
	TypeId cls = type_canon(type_unconst_of(type));

	return type_kind(cls) == TK_CLASS ? cls : TYPE_ERROR;
}

void check_op_visibility (Sema *s, uint32_t method, uint32_t use)
{
	ASTNode *decl = s->ast->nodes + method;

	if (!(decl->flags & NODE_FLAG_PRIV)) return;

	Symbol *fn = s->table.symbols + decl->sym;

	if (s->curr_class == fn->owner) return;
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
			"operator '%s' of class '%s' is private", op_spelling(decl->op),
			types[fn->owner].name);
}

static uint32_t op_operand_param (Sema *s, uint32_t method)
{
	uint32_t args = s->ast->nodes[method].child;
	uint32_t first = s->ast->nodes[args].child;

	return first == NO_NODE ? NO_NODE : s->ast->nodes[first].next_bro;
}

static uint32_t adapt_checked_arg (Sema *s, uint32_t arg, TypeId arg_type, uint32_t param)
{
	TypeId want = s->ast->nodes[param].data_type;

	if (want == TYPE_ERROR || arg_type == TYPE_ERROR || arg_type == want) return arg;
	if (s->ast->nodes[param].flags & NODE_FLAG_REF_LOWERED) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + arg),
				"the left operand of a reversed operator cannot be passed"
				" by reference");
		return arg;
	}
	if (sema_array_slice_compat(arg_type, want)) return arg;

	int ok;
	uint32_t cast = force_cast(s, arg, arg_type, want, &ok);

	if (!ok)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + cast),
				"argument type %s does not match parameter type %s;"
				" use an explicit cast with 'as'",
				type_display_name(arg_type), types[want].name);
	if (type_kind(arg_type) == TK_ARRAY || type_kind(arg_type) == TK_SLICE)
		agg_untrack_root(s, cast);
	return cast;
}

static TypeId reverse_op_call (Sema *s, uint32_t idx, uint32_t receiver, TypeId cls,
		uint32_t arg, TypeId arg_type, uint32_t method)
{
	uint32_t sym = s->ast->nodes[method].sym;
	TypeId ptr = type_ptr_to(cls);
	uint32_t operand = op_operand_param(s, method);

	if (ptr == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + method),
				"too many types");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (operand != NO_NODE) arg = adapt_checked_arg(s, arg, arg_type, operand);
	s->ast->nodes[arg].next_bro = NO_NODE;
	uint32_t recv = make_receiver(s, receiver, ptr);
	const char *spelling = op_spelling(s->ast->nodes[method].op);
	ASTNode *call = s->ast->nodes + idx;
	Symbol *fn = s->table.symbols + sym;

	call->type = NODE_FN_CALL;
	call->flags &= ~NODE_FLAG_INDIRECT;
	call->sym = sym;
	call->str = (char *)spelling;
	call->len = (uint16_t)strlen(spelling);
	call->op = 0;
	s->ast->nodes[recv].next_bro = arg;
	call->child = recv;
	if (sym != s->curr_fn) fn->read = 1;
	call->data_type = type_unconst_of(fn->type);
	return call->data_type;
}

static inline int commutative_op (uint8_t op)
{
	return op == OP_ADD || op == OP_MUL || op == OP_AND || op == OP_OR
			|| op == OP_XOR || op == OP_EQ || op == OP_NE;
}

TypeId sema_class_reverse (Sema *s, uint32_t idx, uint32_t receiver, TypeId receiver_type,
		uint32_t arg, TypeId arg_type, uint8_t op, int *handled)
{
	TypeId cls = class_of(receiver_type);

	*handled = 0;
	if (cls == TYPE_ERROR || !commutative_op(op)) return TYPE_ERROR;
	uint32_t method = class_op_node(s, cls, op);

	if (method == NO_NODE) return TYPE_ERROR;
	*handled = 1;
	check_op_visibility(s, method, idx);
	return reverse_op_call(s, idx, receiver, cls, arg, arg_type, method);
}

TypeId sema_class_operator (Sema *s, uint32_t idx, uint32_t receiver, TypeId type,
		uint32_t args, uint8_t op, int *handled)
{
	TypeId cls = class_of(type);

	*handled = 0;
	if (op == OP_NONE || cls == TYPE_ERROR) return TYPE_ERROR;
	*handled = 1;
	uint32_t method = class_op_node(s, cls, op);

	if (method == NO_NODE) {
		const char *qual = unary_canon(op) ? "unary " : "";

		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"class '%s' has no %soperator '%s'", types[cls].name, qual,
				op_spelling(op));
		if (args != NO_NODE) check_expr(s, args);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	check_op_visibility(s, method, idx);
	return sema_op_call(s, idx, receiver, cls, args, method);
}

static uint32_t make_not_node (Sema *s, uint32_t child, uint16_t line, uint16_t col)
{
	ASTNode raw = {0};

	raw.type = NODE_NOT_L;
	raw.child = child;
	raw.next_bro = NO_NODE;
	raw.data_type = TYPE_i64;
	raw.line = line;
	raw.col = col;
	return sema_new_node(s, raw);
}

static uint32_t truth_zero_literal (Sema *s, uint16_t line, uint16_t col)
{
	ASTNode raw = {0};

	raw.type = NODE_LIT_i64;
	raw.next_bro = NO_NODE;
	raw.line = line;
	raw.col = col;
	return sema_new_node(s, raw);
}

static int op_takes_zero (Sema *s, uint32_t method)
{
	uint32_t operand = op_operand_param(s, method);

	return operand != NO_NODE && is_integer_operand(s->ast->nodes[operand].data_type);
}

static TypeId move_truth_into (Sema *s, uint32_t idx, uint32_t result, uint32_t next)
{
	ASTNode moved = s->ast->nodes[result];

	moved.next_bro = next;
	s->ast->nodes[idx] = moved;
	return s->ast->nodes[idx].data_type;
}

static uint32_t truth_call (Sema *s, uint32_t value, TypeId cls, uint32_t zero,
		uint32_t method, uint16_t line, uint16_t col)
{
	ASTNode raw = {0};

	raw.type = NODE_ERROR;
	raw.next_bro = NO_NODE;
	raw.line = line;
	raw.col = col;
	uint32_t call = sema_new_node(s, raw);

	sema_op_call(s, call, value, cls, zero, method);
	return call;
}

TypeId sema_truth_value (Sema *s, uint32_t idx, TypeId type)
{
	TypeId cls = class_of(type);
	uint32_t method;
	uint32_t zero = NO_NODE;
	int negate = 1;

	if (cls == TYPE_ERROR) return type;
	uint32_t ne = class_op_node(s, cls, OP_NE);
	uint32_t eq = class_op_node(s, cls, OP_EQ);

	int compares = 1;

	if (ne != NO_NODE && op_takes_zero(s, ne)) {
		method = ne;
		negate = 0;
	} else if (eq != NO_NODE && op_takes_zero(s, eq)) {
		method = eq;
	} else {
		method = class_op_node(s, cls, OP_NOT);
		compares = 0;
		if (method == NO_NODE) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
					"class '%s' cannot be used as a truth value; define an"
					" operator '!=', '==' or '!'", types[cls].name);
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}

	uint16_t line = s->ast->nodes[idx].line;
	uint16_t col = s->ast->nodes[idx].col;
	uint32_t next = s->ast->nodes[idx].next_bro;
	ASTNode raw = s->ast->nodes[idx];

	raw.next_bro = NO_NODE;
	uint32_t value = sema_new_node(s, raw);

	if (compares) zero = truth_zero_literal(s, line, col);
	check_op_visibility(s, method, idx);
	uint32_t result = truth_call(s, value, cls, zero, method, line, col);

	if (negate) result = make_not_node(s, result, line, col);
	return move_truth_into(s, idx, result, next);
}

void maybe_copy_init (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId cls = n->child == NO_NODE ? TYPE_ERROR : class_of(n->data_type);

	if (cls == TYPE_ERROR || (n->flags & NODE_FLAG_STATIC)
			|| s->ast->nodes[n->child].type == NODE_CTOR_CALL)
		return;
	if (type_canon(type_unconst_of(s->ast->nodes[n->child].data_type)) != cls) return;
	uint32_t method = class_op_node(s, cls, OP_ASSIGN);

	if (method == NO_NODE) return;
	uint32_t init = n->child;
	ASTNode *in = s->ast->nodes + init;
	ASTNode ctor = {0};

	check_op_visibility(s, method, init);
	ctor.type = NODE_CTOR_CALL;
	ctor.sym = s->ast->nodes[method].sym;
	ctor.child = init;
	ctor.next_bro = NO_NODE;
	ctor.data_type = cls;
	ctor.line = in->line;
	ctor.col = in->col;
	uint32_t call = sema_new_node(s, ctor);

	s->ast->nodes[idx].child = call;
}

typedef struct MethodCall {
	uint32_t dot;
	uint32_t base;
	uint32_t args;
	uint32_t method;
	char *name;
	uint16_t len;
	TypeId cls;
} MethodCall;

static int find_method_call (Sema *s, uint32_t idx, MethodCall *mc)
{
	ASTNode *call = s->ast->nodes + idx;

	mc->dot = (uint32_t)call->u64;
	if (mc->dot == NO_NODE || s->ast->nodes[mc->dot].type != NODE_DOT) return 0;
	mc->base = s->ast->nodes[mc->dot].child;
	mc->name = s->ast->nodes[mc->dot].str;
	mc->len = s->ast->nodes[mc->dot].len;
	mc->args = call->child;

	TypeId base_type = check_expr(s, mc->base);

	if (base_type == TYPE_ERROR) return -1;
	mc->cls = receiver_class(base_type);
	if (mc->cls == TYPE_ERROR) return 0;

	mc->method = class_method_node(s, mc->cls, mc->name, mc->len);
	if (mc->method != NO_NODE) return 1;
	if (find_field(mc->cls, mc->name, mc->len)) return 0;
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + mc->dot),
			"class '%s' has no method '%.*s'", types[mc->cls].name,
			(int)mc->len, mc->name);
	return -1;
}

static TypeId dispatch_method (Sema *s, uint32_t idx, MethodCall *mc)
{
	uint32_t sym = s->ast->nodes[mc->method].sym;
	uint32_t receiver = NO_NODE;

	if (reject_special_call(s, mc->dot, sym)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	check_method_visibility(s, sym, mc->dot);
	if (class_fn_has_self(s, mc->method)) {
		TypeId ptr = type_ptr_to(mc->cls);

		if (ptr == TYPE_ERROR) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + mc->dot),
					"too many types");
			s->ast->nodes[idx].data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
		receiver = make_receiver(s, mc->base, ptr);
	} else if (expr_has_effect(s, mc->base)) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + mc->base),
				"the receiver of an associated function cannot have side effects");
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	ASTNode *call = s->ast->nodes + idx;

	call->str = mc->name;
	call->len = mc->len;
	return finish_method_call(s, idx, sym, receiver, mc->args);
}

static uint32_t ctor_args_node (Sema *s, uint32_t init)
{
	uint32_t params = s->ast->nodes[init].child;
	uint32_t first = s->ast->nodes[params].child;
	uint32_t rest = first;

	if (first != NO_NODE && (s->ast->nodes[first].flags & NODE_FLAG_SELF))
		rest = s->ast->nodes[first].next_bro;
	ASTNode args = s->ast->nodes[params];
	args.child = rest;
	return sema_new_node(s, args);
}

static uint32_t ctor_fn_node (Sema *s, uint32_t init, uint32_t args_node)
{
	ASTNode fn = s->ast->nodes[init];

	fn.child = args_node;
	return sema_new_node(s, fn);
}

void check_ctor_args (Sema *s, uint32_t call, uint32_t init)
{
	uint32_t args = ctor_args_node(s, init);
	uint32_t fn = ctor_fn_node(s, init, args);

	check_call_args(s, call, fn);
}

TypeId check_ctor_call (Sema *s, uint32_t idx, TypeId cls)
{
	uint32_t init = class_init_node(s, cls);
	uint32_t argc = 0;

	for (uint32_t a = s->ast->nodes[idx].child; a != NO_NODE;
			a = s->ast->nodes[a].next_bro)
		argc++;

	if (init != NO_NODE) {
		check_ctor_args(s, idx, init);
	} else if (argc) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"class '%s' has no initializer; '%.*s(...)' cannot take arguments",
				types[cls].name, (int)s->ast->nodes[idx].len,
				s->ast->nodes[idx].str);
		for (uint32_t a = s->ast->nodes[idx].child; a != NO_NODE; ) {
			uint32_t next = s->ast->nodes[a].next_bro;

			check_expr(s, a);
			a = next;
		}
	}

	ASTNode *n = s->ast->nodes + idx;

	n->type = NODE_CTOR_CALL;
	n->sym = init == NO_NODE ? NO_SYMBOL : s->ast->nodes[init].sym;
	n->data_type = cls;
	return cls;
}

TypeId check_method_call (Sema *s, uint32_t idx, int *handled)
{
	MethodCall mc;
	int found = find_method_call(s, idx, &mc);

	*handled = found != 0;
	if (found < 0) s->ast->nodes[idx].data_type = TYPE_ERROR;
	if (found <= 0) return TYPE_ERROR;
	return dispatch_method(s, idx, &mc);
}
