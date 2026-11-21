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

typedef struct {
	TypeId type;
	uint64_t size;
	uint64_t align;
} OperandInfo;

static void info_from_type (OperandInfo *info, TypeId type)
{
	info->type = type;
	info->size = types[type].size;
	info->align = type_align(type_repr(type));
}

static int operand_named_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	uint32_t sym_idx = sema_lookup(s, n->str, n->len);

	if (sym_idx == NO_SYMBOL) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' is not declared", (int)n->len, n->str);
		return 0;
	}

	Symbol *sym = s->table.symbols + sym_idx;
	if (sym->kind == SYMBOL_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of %s cannot be a function", op);
		return 0;
	}
	if (sym->kind != SYMBOL_VAR && sym->kind != SYMBOL_PARAM
			&& sym->kind != SYMBOL_TYPE && sym->kind != SYMBOL_ENUM)
		return 0;

	sym->read = 1;
	if (sym->type != TYPE_ERROR
			&& (sym->kind == SYMBOL_TYPE || sym->kind == SYMBOL_ENUM))
		types[sym->type].used = 1;
	info->type = sym->type;
	if (sym->by_ref && type_kind(info->type) == TK_PTR)
		info->type = types[info->type].base;
	return 1;
}

static int operand_field_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op);
static int operand_ptr_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op);
static int operand_index_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op);

static int operand_expr_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	switch (n->type)
	{
	case NODE_ID:
		if (!operand_named_info(s, n, info, op)) return 0;
		if (info->type == TYPE_ERROR) return 1;
		info_from_type(info, info->type);
		return 1;
	case NODE_LIT_STR:
		info_from_type(info, TYPE_STR);
		info->size = (uint64_t)n->len + 1;
		info->align = 1;
		return 1;
	case NODE_LIT_NULL: info_from_type(info, TYPE_NULL); return 1;
	case NODE_LIT_i64: info_from_type(info, TYPE_i64); return 1;
	case NODE_LIT_u64: info_from_type(info, TYPE_u64); return 1;
	case NODE_LIT_CHAR: info_from_type(info, TYPE_i8); return 1;
	case NODE_DEREF:
		return operand_ptr_info(s, n, info, op);
	case NODE_INDEX:
		return operand_index_info(s, n, info, op);
	case NODE_DOT:
		return operand_field_info(s, n, info, op);
	default:
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of %s must be a variable, dereference, index, string literal or type", op);
		return 0;
	}
}

static int operand_ptr_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	const char *what = n->type == NODE_INDEX ? "index" : "dereference";
	OperandInfo base;

	if (!operand_expr_info(s, s->ast->nodes + n->child, &base, op)) return 0;
	if (base.type == TYPE_ERROR) {
		info->type = TYPE_ERROR;
		return 1;
	}
	TypeKind kind = type_kind(base.type);
	if (kind == TK_ARRAY || kind == TK_SLICE) {
		info_from_type(info, types[base.type].base);
		return 1;
	}
	if (kind != TK_PTR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot %s a value of type %s", what, type_display_name(base.type));
		return 0;
	}
	if (types[base.type].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot %s 'ptr void'; cast it to a typed pointer first", what);
		return 0;
	}
	info_from_type(info, types[base.type].base);
	return 1;
}

static int operand_index_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	ASTNode *child = s->ast->nodes + n->child;

	if (child->type == NODE_ID) {
		uint32_t sym_idx = sema_lookup(s, child->str, child->len);
		Symbol *sym = sym_idx == NO_SYMBOL ? 0 : s->table.symbols + sym_idx;

		if (sym && (sym->kind == SYMBOL_TYPE || sym->kind == SYMBOL_ENUM)) {
			uint32_t idx_node = child->next_bro;
			uint64_t dim = 0;
			TypeId base = sym->type;

			if (idx_node == NO_NODE || !dim_value(s, idx_node, &dim)) {
				error_report(s->err, ERR_ERROR, node_loc(n),
						"operand of %s must be a constant size", op);
				return 0;
			}
			if (base != TYPE_ERROR) types[base].used = 1;
			info_from_type(info, type_array_of(base, (uint32_t)dim));
			return 1;
		}
	}
	return operand_ptr_info(s, n, info, op);
}

static int operand_field_info (Sema *s, ASTNode *n, OperandInfo *info, const char *op)
{
	OperandInfo left;

	if (!operand_expr_info(s, s->ast->nodes + n->child, &left, op)) return 0;
	if (left.type == TYPE_ERROR) {
		info->type = TYPE_ERROR;
		return 1;
	}
	TypeId st = left.type;
	if (type_kind(st) == TK_PTR) st = types[st].base;
	if (type_kind(st) != TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"operand of %s must be a variable, dereference, index, string literal or type", op);
		return 0;
	}
	const StructField *f = find_field(st, n->str, n->len);
	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(n), "struct '%s' has no field '%.*s'",
				types[st].name, (int)n->len, n->str);
		return 0;
	}
	info_from_type(info, f->type);
	return 1;
}

static int operand_info (Sema *s, uint32_t idx, OperandInfo *info, const char *op)
{
	ASTNode *n = s->ast->nodes + idx;

	info->type = TYPE_ERROR;
	if (n->child == NO_NODE) {
		TypeId type = resolve_type_node(s, n);

		n = s->ast->nodes + idx;
		if (type == TYPE_ERROR) return 0;
		if (type == TYPE_VOID) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"%s cannot be applied to type void", op);
			return 0;
		}
		info_from_type(info, type);
		return 1;
	}
	if (!operand_expr_info(s, s->ast->nodes + n->child, info, op)) return 0;
	if (info->type == TYPE_ERROR) return 0;
	if (info->type == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"%s cannot be applied to a value of type void", op);
		return 0;
	}
	return 1;
}

static TypeId fold_layout_lit (Sema *s, uint32_t idx, uint64_t value)
{
	ASTNode *n = s->ast->nodes + idx;

	n->type = NODE_LIT_i64;
	n->i64 = (int64_t)value;
	n->data_type = TYPE_UNTYPED_INT;
	n->child = NO_NODE;
	n->type_node = NO_TYPE_NODE;
	n->sym = NO_SYMBOL;
	return TYPE_UNTYPED_INT;
}

static TypeId err_incomplete_type (Sema *s, uint32_t idx, TypeId type)
{
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
			"type '%s' is incomplete", types[type].name);
	s->ast->nodes[idx].data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

TypeId check_sizeof (Sema *s, uint32_t idx)
{
	OperandInfo info;

	if (!operand_info(s, idx, &info, "sizeof")) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return fold_layout_lit(s, idx, info.size);
}

TypeId check_alignof (Sema *s, uint32_t idx)
{
	OperandInfo info;

	if (!operand_info(s, idx, &info, "alignof")) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId repr = type_repr(info.type);
	if (type_kind(repr) == TK_STRUCT && types[repr].layout != 2)
		return err_incomplete_type(s, idx, repr);
	return fold_layout_lit(s, idx, info.align);
}

static int offsetof_step (Sema *s, ASTNode *field, TypeId *type, uint64_t *off, int last)
{
	const StructField *f = find_field(*type, field->str, field->len);

	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(field),
				"struct '%s' has no field '%.*s'", types[*type].name,
				(int)field->len, field->str);
		return 0;
	}
	if (f->bits) {
		error_report(s->err, ERR_ERROR, node_loc(field),
				"offsetof cannot be applied to bitfield '%.*s'",
				(int)field->len, field->str);
		return 0;
	}
	if (!last && type_kind(f->type) != TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(field),
				"offsetof cannot traverse field '%.*s' of type %s",
				(int)field->len, field->str, types[f->type].name);
		return 0;
	}

	*off += f->offset;
	*type = f->type;
	return 1;
}

TypeId check_offsetof (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = resolve_type_node(s, n);

	n = s->ast->nodes + idx;
	if (type == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (type_kind(type) != TK_STRUCT) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"offsetof requires a struct or union type, not '%s'",
				types[type].name);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (types[type].layout != 2) return err_incomplete_type(s, idx, type);

	uint64_t off = 0;
	for (uint32_t seg = n->child; seg != NO_NODE; seg = s->ast->nodes[seg].next_bro) {
		int last = s->ast->nodes[seg].next_bro == NO_NODE;

		if (!offsetof_step(s, s->ast->nodes + seg, &type, &off, last)) {
			n->data_type = TYPE_ERROR;
			return TYPE_ERROR;
		}
	}

	return fold_layout_lit(s, idx, off);
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
	if (type_kind(types[type].base) == TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot dereference a function pointer; call it directly");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	n->data_type = types[type].base;
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
	if (type != TYPE_ERROR && child->sym != NO_SYMBOL)
		s->table.symbols[child->sym].addressed = 1;
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
	if (child->type == NODE_INDEX)
		return check_index(s, n->child);
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
	if (type_kind(type) == TK_STRUCT) return check_field(s, idx, type);
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
				"struct '%s' has no field '%.*s'", types[st].name,
				(int)n->len, n->str);
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (f->bits) n->flags |= NODE_FLAG_BITFIELD;
	n->data_type = f->type;
	agg_member_read(s, idx);
	return f->type;
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
	if (type_kind(type) == TK_PTR && type_kind(types[type].base) == TK_STRUCT) {
		uint32_t deref;

		warn_null_deref(s, idx, n->child);
		type = types[type].base;
		deref = sema_wrap_deref(s, n->child, type);
		s->ast->nodes[idx].child = deref;
		n = s->ast->nodes + idx;
	}
	if (type_kind(type) == TK_STRUCT) return check_field(s, idx, type);
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

	if (kind == TK_STRUCT || (kind == TK_PTR
			&& type_kind(types[sym_type].base) == TK_STRUCT))
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

TypeId check_index (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t base = n->child;
	uint32_t index = s->ast->nodes[base].next_bro;
	TypeId ptr = check_member_base(s, base);
	TypeId off = check_expr(s, index);

	n = s->ast->nodes + idx;
	if (ptr == TYPE_ERROR || off == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}

	TypeKind kind = type_kind(ptr);
	if (kind != TK_PTR && kind != TK_ARRAY && kind != TK_SLICE) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index a value of type %s", type_display_name(ptr));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (kind == TK_PTR && types[ptr].base == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index 'ptr void'; cast it to a typed pointer first");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (kind == TK_PTR && type_kind(types[ptr].base) == TK_FN) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot index a function pointer; call it directly");
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
		uint32_t count = types[ptr].count;

		if (lit_node_bits(s, s->ast->nodes + index, &bits)) {
			int neg = types[index_type].sign && (int64_t)bits < 0;

			if (neg || bits >= count)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"index %lld is out of bounds for an array of size %u",
						(long long)(int64_t)bits, count);
		}
	}

	n->data_type = types[ptr].base;
	if (kind == TK_PTR) warn_null_deref(s, idx, base);
	agg_member_read(s, idx);
	return n->data_type;
}
