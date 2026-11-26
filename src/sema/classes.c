#include "sema/common.h"

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
		if (fn->sym == NO_SYMBOL) continue;
		if (fn->len == len && memcmp(fn->str, name, len) == 0) return m;
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
	method->fn_type = build_fn_value(s, fn_idx);
	if (find_field(owner, fn->str, fn->len))
		error_report(s->err, ERR_ERROR, node_loc(fn),
				"class '%s' already has a field named '%.*s'",
				ct->name, (int)fn->len, fn->str);
}

static void declare_class_method (Sema *s, uint32_t fn_idx, TypeId owner)
{
	ASTNode *fn = s->ast->nodes + fn_idx;
	int is_init = (fn->flags & NODE_FLAG_INIT) != 0;
	int is_destroy = (fn->flags & NODE_FLAG_DESTROY) != 0;

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
