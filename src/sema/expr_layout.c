#include "sema/common.h"
#include "sema/expr_internal.h"
#include <string.h>

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
	TypeId repr = type_ptr_repr(base.type);
	if (repr == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot %s a value of type %s", what, type_display_name(base.type));
		return 0;
	}
	if (type_unconst_of(types[repr].base) == TYPE_VOID) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot %s 'ptr void'; cast it to a typed pointer first", what);
		return 0;
	}
	info_from_type(info, types[repr].base);
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
	TypeId repr = type_ptr_repr(st);
	if (repr != TYPE_ERROR) st = types[repr].base;
	if (!type_is_struct_like(st)) {
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
	if (type_is_struct_like(repr) && types[repr].layout != 2)
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
	if (!last && !type_is_struct_like(f->type)) {
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
	if (!type_is_struct_like(type)) {
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
