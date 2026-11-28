#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

TypeId check_index (Sema *s, uint32_t idx);
static TypeId check_len_field (Sema *s, uint32_t idx, uint32_t left, TypeId type);
static TypeId check_dot_value (Sema *s, uint32_t idx);
static TypeId check_field (Sema *s, uint32_t idx, TypeId st);

static TypeId check_member_base (Sema *s, uint32_t child)
{
	ASTNode *n = s->ast->nodes + child;

	if (n->type != NODE_ID) return check_expr(s, child);

	uint32_t sym_idx = symtab_find(&s->table, n->str, n->len);
	if (sym_idx == NO_SYMBOL || !s->table.symbols[sym_idx].agg_slots)
		return check_expr(s, child);

	TypeId type = check_var_ref(s, child, 1);
	n = s->ast->nodes + child;
	if (type != TYPE_ERROR && n->sym != NO_SYMBOL && child != s->write_root) {
		Symbol *sym = &s->table.symbols[n->sym];

		sym->read = 1;
		sym->pending_write = 0;
	}
	return type;
}

TypeId check_deref (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->flags & NODE_FLAG_REF) return n->data_type;
	TypeId type = check_expr(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId repr = type_ptr_repr(type);
	if (repr == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot dereference a value of type %s", type_display_name(type));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_unconst_of(types[repr].base) == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot dereference 'ptr void'; cast it to a typed pointer first");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(types[repr].base) == TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot dereference a function pointer; call it directly");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = types[repr].base;
	warn_null_deref(s, idx, n->child);
	return n->data_type;
}

static TypeId check_addr_id (Sema *s, uint32_t idx, uint32_t saved_addr, int *finalized)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *child = s->ast->nodes + n->child;
	TypeId type = check_var_ref(s, n->child, 0);

	n = s->ast->nodes + idx;
	child = s->ast->nodes + n->child;
	if (type != TYPE_ERROR && child->sym != NO_SYMBOL
			&& s->table.symbols[child->sym].kind == SYMBOL_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is a function; use it directly as a pointer value",
				(int)child->len, child->str);
		n->data_type = TYPE_ERROR;
		*finalized = 1;
		return TYPE_ERROR;
	}
	if (type != TYPE_ERROR && child->sym != NO_SYMBOL
			&& s->table.symbols[child->sym].by_ref) {
		s->addr_operand = saved_addr;
		n->type = NODE_ID;
		n->child = NO_NODE;
		n->sym = child->sym;
		n->str = child->str;
		n->len = child->len;
		n->data_type = type;
		mark_read(s, idx);
		*finalized = 1;
		return type;
	}
	if (type != TYPE_ERROR && child->sym != NO_SYMBOL) {
		Symbol *sym = s->table.symbols + child->sym;

		sym->addressed = 1;
		sym->assigned = 1;
		sym->null_known = 0;
	}
	mark_read(s, n->child);
	return type;
}

static TypeId check_addr_dot (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_dot(s, n->child);

	n = s->ast->nodes + idx;
	if (type != TYPE_ERROR && (s->ast->nodes[n->child].flags & NODE_FLAG_BITFIELD)) {
		if (s->ref_arg)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot take a reference to bitfield '%.*s'",
					(int)s->ast->nodes[n->child].len, s->ast->nodes[n->child].str);
		else error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot take the address of bitfield '%.*s'",
				(int)s->ast->nodes[n->child].len, s->ast->nodes[n->child].str);
		return TYPE_ERROR;
	}
	if (type != TYPE_ERROR && s->ast->nodes[n->child].type != NODE_DOT) {
		if (s->ref_arg)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"argument for reference parameter must be a variable");
		else error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of '$' must be a variable, a dereference or an index");
		return TYPE_ERROR;
	}
	return type;
}

static TypeId check_addr_child (Sema *s, uint32_t idx, uint32_t saved_addr, int *finalized)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *child = s->ast->nodes + n->child;

	if (child->type == NODE_ID)
		return check_addr_id(s, idx, saved_addr, finalized);
	if (child->type == NODE_DEREF)
		return check_deref(s, n->child);
	if (child->type == NODE_INDEX) {
		uint32_t ci = n->child;
		TypeId type = check_index(s, ci);
		ASTNode *now = s->ast->nodes + ci;

		if (now->type == NODE_FN_CALL && now->op == OP_INDEX) {
			n = s->ast->nodes + idx;
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot take the address of a value returned by"
					" operator '[]'");
			n->data_type = TYPE_ERROR;
			*finalized = 1;
			return TYPE_ERROR;
		}
		return type;
	}
	if (child->type == NODE_DOT)
		return check_addr_dot(s, idx);

	s->addr_operand = saved_addr;
	error_report(s->err, ERR_ERROR, node_loc(n),
			"operand of '$' must be a variable, a dereference or an index");
	n->data_type = TYPE_ERROR;
	*finalized = 1;
	return TYPE_ERROR;
}

static TypeId check_addr_finish (Sema *s, uint32_t idx, TypeId type)
{
	ASTNode *n = s->ast->nodes + idx;

	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE) {
		if (s->ref_arg)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"cannot take a reference to an array or slice");
		else error_report(s->err, ERR_ERROR, node_loc(n),
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
	agg_untrack_root(s, n->child);
	return n->data_type;
}

TypeId check_addr (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->flags & NODE_FLAG_TEMP_ADDR) return n->data_type;

	uint32_t saved_addr = s->addr_operand;

	s->addr_operand = 1;
	int finalized = 0;
	TypeId type = check_addr_child(s, idx, saved_addr, &finalized);
	s->addr_operand = saved_addr;
	if (finalized) return type;
	return check_addr_finish(s, idx, type);
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

static TypeId check_dot_index (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_index(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_is_struct_like(type)) return check_field(s, idx, type);
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE)
		return check_len_field(s, idx, n->child, type);

	error_report(s->err, ERR_ERROR, node_loc(n),
			"a value of type %s is not an array or a slice", type_display_name(type));
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

static TypeId check_dot_var (Sema *s, uint32_t idx, Symbol *sym)
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

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return check_len_field(s, idx, n->child, type);
}

static TypeId check_dot_enum (Sema *s, uint32_t idx, Symbol *sym)
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

static TypeId check_field (Sema *s, uint32_t idx, TypeId st)
{
	ASTNode *n = s->ast->nodes + idx;
	const StructField *f = find_field(st, n->str, n->len);

	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"%s '%s' has no field '%.*s'",
				type_kind(st) == TK_CLASS ? "class" : "struct",
				types[st].name, (int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (f->bits) n->flags |= NODE_FLAG_BITFIELD;

	TypeId ft = f->type;

	if (type_is_const(st) && !type_is_const(ft)) ft = type_const_of(ft);
	n->data_type = ft;
	agg_member_read(s, idx);
	return ft;
}

static TypeId check_dot_value (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = check_member_base(s, n->child);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId repr = type_ptr_repr(type);
	if (repr != TYPE_ERROR && type_is_struct_like(types[repr].base)) {
		uint32_t deref;

		warn_null_deref(s, idx, n->child);
		type = types[repr].base;
		deref = sema_wrap_deref(s, n->child, type);
		s->ast->nodes[idx].child = deref;
		n = s->ast->nodes + idx;
	}
	if (type_is_struct_like(type)) return check_field(s, idx, type);
	if (type_kind(type) == TK_ARRAY || type_kind(type) == TK_SLICE)
		return check_len_field(s, idx, n->child, type);
	if (n->len == 3 && memcmp(n->str, "len", 3) == 0)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"a value of type %s is not an array or a slice",
				type_display_name(type));
	else error_report(s->err, ERR_ERROR, node_loc(n),
			"a value of type %s is not an enum type",
			type_display_name(type));
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

TypeId check_dot (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	ASTNode *left = s->ast->nodes + n->child;

	if (left->type == NODE_INDEX)
		return check_dot_index(s, idx);

	if (left->type != NODE_ID) return check_dot_value(s, idx);

	uint32_t sym_idx = sema_lookup(s, left->str, left->len);
	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(left),
				"'%.*s' is not declared", (int)left->len, left->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	Symbol *sym = s->table.symbols + sym_idx;
	TypeId sym_type = sym->type;

	if (sym->by_ref && type_kind(sym_type) == TK_PTR)
		sym_type = types[sym_type].base;
	TypeKind kind = type_kind(sym_type);

	TypeId sym_repr = type_ptr_repr(sym_type);
	if ((kind == TK_STRUCT || kind == TK_CLASS) || (sym_repr != TYPE_ERROR
			&& type_is_struct_like(types[sym_repr].base)))
		return check_dot_value(s, idx);
	if (sym->kind == SYMBOL_VAR || sym->kind == SYMBOL_PARAM)
		return check_dot_var(s, idx, sym);
	if (sym->kind == SYMBOL_ENUM || (sym->kind == SYMBOL_TYPE
			&& type_kind(type_repr(sym->type)) == TK_ENUM))
		return check_dot_enum(s, idx, sym);
	error_report(s->err, ERR_ERROR, node_loc(left),
			"'%.*s' is not an enum type", (int)left->len, left->str);
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

static uint32_t index_arg_count (Sema *s, uint32_t first)
{
	uint32_t count = 0;

	for (uint32_t i = first; i != NO_NODE; i = s->ast->nodes[i].next_bro)
		count++;
	return count;
}

static void check_index_args (Sema *s, uint32_t first)
{
	for (uint32_t i = first; i != NO_NODE; i = s->ast->nodes[i].next_bro)
		check_expr(s, i);
}

static uint32_t index_op_params (Sema *s, uint32_t method)
{
	uint32_t args = s->ast->nodes[method].child;
	uint32_t count = 0;

	for (uint32_t p = s->ast->nodes[args].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro)
		count++;
	return count - 1;
}

static TypeId check_index_value (Sema *s, uint32_t idx, uint32_t base,
		TypeId cls, uint32_t first, uint32_t method)
{
	TypeId result = sema_op_call(s, idx, base, cls, first, method);

	if (result != TYPE_ERROR) s->ast->nodes[idx].op = OP_INDEX;
	return result;
}

static TypeId check_index_lvalue (Sema *s, uint32_t idx, uint32_t base,
		TypeId cls, uint32_t first, uint32_t method, TypeId elem)
{
	ASTNode call = {0};
	call.type = NODE_FN_CALL;
	call.line = s->ast->nodes[idx].line;
	call.col = s->ast->nodes[idx].col;
	call.child = NO_NODE;
	call.next_bro = NO_NODE;
	call.sym = NO_SYMBOL;
	call.type_node = NO_TYPE_NODE;
	uint32_t call_idx = sema_new_node(s, call);
	TypeId result = sema_op_call(s, call_idx, base, cls, first, method);

	if (result == TYPE_ERROR) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	uint32_t next = s->ast->nodes[idx].next_bro;
	ASTNode *n = s->ast->nodes + idx;

	n->type = NODE_DEREF;
	n->op = 0;
	n->flags = 0;
	n->type_node = NO_TYPE_NODE;
	n->sym = NO_SYMBOL;
	n->child = call_idx;
	n->next_bro = next;
	n->data_type = elem;
	return elem;
}

static TypeId check_index_op (Sema *s, uint32_t idx, uint32_t base,
		TypeId cls, uint32_t first)
{
	uint32_t method = class_op_node(s, cls, OP_INDEX);
	uint32_t got = index_arg_count(s, first);
	ASTNode *n = s->ast->nodes + idx;

	if (method == NO_NODE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"class '%s' has no operator '[]'", types[cls].name);
		check_index_args(s, first);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	uint32_t want = index_op_params(s, method);
	if (got != want) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator '[]' of class '%s' expects %u index%s, got %u",
				types[cls].name, want, want == 1 ? "" : "es", got);
		check_index_args(s, first);
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId ret = s->table.symbols[s->ast->nodes[method].sym].type;
	TypeId repr = type_ptr_repr(ret);
	if (repr == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operator '[]' must return a pointer type");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId elem = types[repr].base;
	TypeId elem_u = type_canon(type_unconst_of(elem));
	if (elem_u == TYPE_VOID || type_kind(elem_u) == TK_FN)
		return check_index_value(s, idx, base, cls, first, method);
	return check_index_lvalue(s, idx, base, cls, first, method, elem);
}

static TypeId check_native_index (Sema *s, uint32_t idx, uint32_t base,
		uint32_t index, TypeId ptr)
{
	TypeId off = check_expr(s, index);
	ASTNode *n = s->ast->nodes + idx;

	if (ptr == TYPE_ERROR || off == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	TypeId repr = type_ptr_repr(ptr);
	TypeKind kind = repr != TYPE_ERROR ? TK_PTR : type_kind(ptr);
	TypeId target = repr != TYPE_ERROR ? repr : ptr;
	if (kind != TK_PTR && kind != TK_ARRAY && kind != TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index a value of type %s", type_display_name(ptr));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (kind == TK_PTR && type_unconst_of(types[target].base) == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index 'ptr void'; cast it to a typed pointer first");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (kind == TK_PTR && type_kind(types[target].base) == TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index a function pointer; call it directly");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (s->ast->nodes[index].next_bro != NO_NODE) {
		check_index_args(s, s->ast->nodes[index].next_bro);
		n = s->ast->nodes + idx;
		error_report(s->err, ERR_ERROR, node_loc(n),
				"native indexing takes a single index; use 'a[i][j]'");
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
	n = s->ast->nodes + idx;

	if (kind == TK_ARRAY) {
		uint64_t bits;
		uint32_t count = types[target].count;

		if (lit_node_bits(s, s->ast->nodes + index, &bits)) {
			int neg = types[index_type].sign && (int64_t)bits < 0;

			if (neg || bits >= count)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"index %lld is out of bounds for an array of size %u",
						(long long)(int64_t)bits, count);
		}
	}

	n->data_type = types[target].base;
	if (kind == TK_PTR) warn_null_deref(s, idx, base);
	agg_member_read(s, idx);
	return n->data_type;
}

TypeId check_index (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t base = n->child;
	uint32_t index = s->ast->nodes[base].next_bro;
	TypeId ptr = check_member_base(s, base);
	TypeId value = type_canon(type_unconst_of(ptr));

	if (type_kind(value) == TK_CLASS)
		return check_index_op(s, idx, base, value, index);
	return check_native_index(s, idx, base, index, ptr);
}
