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
		if (fn->len == len && memcmp(fn->str, name, len) == 0) return m;
	}
	return NO_NODE;
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

static void declare_class_method (Sema *s, uint32_t fn_idx, TypeId owner)
{
	mark_class_self(s, fn_idx, owner);
	resolve_fn_types(s, fn_idx);

	ASTNode *fn = s->ast->nodes + fn_idx;
	uint32_t ret = s->ast->nodes[fn->child].next_bro;
	Type *ct = types + owner;
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

		if (s->ast->nodes[m].type == NODE_FN_DEC) check_fn_dec(s, m);
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

TypeId check_method_call (Sema *s, uint32_t idx, int *handled)
{
	MethodCall mc;
	int found = find_method_call(s, idx, &mc);

	*handled = found != 0;
	if (found < 0) s->ast->nodes[idx].data_type = TYPE_ERROR;
	if (found <= 0) return TYPE_ERROR;
	return dispatch_method(s, idx, &mc);
}
