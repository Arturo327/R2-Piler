#include "r2i/internal.h"

#include <string.h>

#include "mangle/mangle.h"

static const char *type_kind_sym (ASTNode *n)
{
	switch (n->type)
	{
	case NODE_STRUCT_DEC: return (n->flags & NODE_FLAG_UNION) ? "u" : "s";
	case NODE_CLASS_DEC: return "s";
	case NODE_ENUM_DEC: return "e";
	case NODE_TYPE_DEC: return "a";
	case NODE_NEWTYPE_DEC: return "n";
	default: return NULL;
	}
}

void r2i_build_types (R2i *r)
{
	AST *ast = r->ir->ast;
	SymbolTable *table = r->ir->symtab;

	r->type_total = type_count;
	r->type_syms = arena_alloc(r->ir->arena, sizeof(char *) * type_count);
	memset(r->type_syms, 0, sizeof(char *) * type_count);
	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		ASTNode *n = ast->nodes + idx;
		const char *kind = type_kind_sym(n);

		if (!kind || n->sym == NO_SYMBOL
				|| (n->flags & NODE_FLAG_IMPORTED)) continue;
		TypeId id = table->symbols[n->sym].type;

		if (id == TYPE_ERROR || id >= type_count) continue;
		if (n->flags & NODE_FLAG_EXPORT) {
			r->type_syms[id] = mangle_plain(r->ir->arena, n->str, n->len);
		} else {
			MangleName m = { .kind = kind, .mod = r->ir->mod,
					.a = n->str, .alen = n->len };
			r->type_syms[id] = mangle_symbol(r->ir->arena, &m);
		}
	}
}

static void r2i_print_builtin (R2i *r, TypeId t)
{
	switch (t)
	{
	case TYPE_VOID: r2i_printf(r, "void"); return;
	case TYPE_STR: r2i_printf(r, "ptr i8"); return;
	case TYPE_NULL: r2i_printf(r, "ptr void"); return;
	case TYPE_UNTYPED_INT: r2i_printf(r, "i64"); return;
	case TYPE_UNTYPED_UINT: r2i_printf(r, "u64"); return;
	case TYPE_UNTYPED_CHAR: r2i_printf(r, "i8"); return;
	case TYPE_ERROR: r2i_printf(r, "void"); return;
	default: r2i_printf(r, "%s", types[t].name); return;
	}
}

static void r2i_print_fn_rets (R2i *r, TypeId t)
{
	Type *fn = types + t;

	if (!fn->ret_count) return;
	if (fn->ret_count == 1) {
		r2i_printf(r, " : ");
		r2i_print_type(r, fn->rets[0]);
		return;
	}
	r2i_printf(r, " : (");
	for (uint16_t i = 0; i < fn->ret_count; i++) {
		if (i) r2i_printf(r, ", ");
		r2i_print_type(r, fn->rets[i]);
	}
	r2i_printf(r, ")");
}

static void r2i_print_fn_type (R2i *r, TypeId t)
{
	Type *fn = types + t;

	r2i_printf(r, "fn(");
	for (uint16_t i = 0; i < fn->param_count; i++) {
		if (i) r2i_printf(r, ", ");
		r2i_print_type(r, fn->params[i]);
	}
	if (fn->fn_variadic) r2i_printf(r, fn->param_count ? ", ..." : "...");
	r2i_printf(r, ")");
	r2i_print_fn_rets(r, t);
}

#define R2I_MAX_RANK 16

static void r2i_print_array_type (R2i *r, TypeId t)
{
	uint32_t counts[R2I_MAX_RANK];
	uint8_t slices[R2I_MAX_RANK];
	int rank = 0;

	while (type_kind(t) == TK_ARRAY || type_kind(t) == TK_SLICE) {
		if (rank < R2I_MAX_RANK) {
			slices[rank] = type_kind(t) == TK_SLICE;
			counts[rank] = types[t].count;
			rank++;
		}
		t = types[t].base;
	}
	r2i_print_type(r, t);
	for (int i = 0; i < rank; i++) {
		if (slices[i]) r2i_printf(r, "[]");
		else r2i_printf(r, "[%u]", counts[i]);
	}
}

void r2i_print_type (R2i *r, TypeId t)
{
	if (t == TYPE_ERROR) {
		r2i_printf(r, "void");
		return;
	}
	if (type_is_const(t)) {
		r2i_printf(r, "const ");
		r2i_print_type(r, type_unconst_of(t));
		return;
	}
	if (t < TYPE_COUNT) {
		r2i_print_builtin(r, t);
		return;
	}
	switch (type_kind(t))
	{
	case TK_PTR:
		r2i_printf(r, "ptr ");
		r2i_print_type(r, types[t].base);
		return;
	case TK_ARRAY: case TK_SLICE:
		r2i_print_array_type(r, t);
		return;
	case TK_FN:
		r2i_print_fn_type(r, t);
		return;
	default:
		if (t < r->type_total && r->type_syms[t])
			r2i_printf(r, "%s", r->type_syms[t]);
		else r2i_printf(r, "%s", types[t].name);
		return;
	}
}

static void r2i_make_field (R2i *r, ASTNode *n, int depth)
{
	r2i_indent(r, depth);
	r2i_printf(r, "%.*s : ", (int)n->len, n->str);
	if (!(n->flags & NODE_FLAG_BITFIELD)) {
		r2i_print_type(r, n->data_type);
	} else if (n->flags & NODE_FLAG_BITFIELD_TYPED) {
		r2i_print_type(r, n->data_type);
		r2i_printf(r, " : %u", (unsigned)n->sym);
	} else {
		r2i_printf(r, "%c%u", n->data_type == TYPE_UNTYPED_INT ? 'i' : 'u',
				(unsigned)n->sym);
	}
	r2i_printf(r, ";\n");
}

static void r2i_make_nested (R2i *r, ASTNode *n, int depth)
{
	r2i_indent(r, depth);
	r2i_printf(r, "%s", (n->flags & NODE_FLAG_UNION) ? "union" : "struct");
	if (n->str) r2i_printf(r, " %.*s", (int)n->len, n->str);
	r2i_printf(r, " {\n");
	for (uint32_t m = n->child; m != NO_NODE; m = r->ir->ast->nodes[m].next_bro) {
		ASTNode *member = r->ir->ast->nodes + m;

		if (member->type == NODE_FIELD_DEC) r2i_make_field(r, member, depth + 1);
		else if (member->type == NODE_STRUCT_DEC) r2i_make_nested(r, member, depth + 1);
	}
	r2i_indent(r, depth);
	r2i_printf(r, "}\n");
}

void r2i_make_struct (R2i *r, ASTNode *n)
{
	Symbol *sym = r->ir->symtab->symbols + n->sym;
	const char *name = r->type_syms[sym->type];

	r2i_printf(r, "%s%s %s %s {\n",
			(n->flags & NODE_FLAG_EXPORT) ? "export" : "pub",
			(n->flags & NODE_FLAG_PACKED) ? " packed" : "",
			(n->flags & NODE_FLAG_UNION) ? "union" : "struct", name);
	for (uint32_t m = n->child; m != NO_NODE; m = r->ir->ast->nodes[m].next_bro) {
		ASTNode *member = r->ir->ast->nodes + m;

		if (member->type == NODE_FIELD_DEC) r2i_make_field(r, member, 1);
		else if (member->type == NODE_STRUCT_DEC) r2i_make_nested(r, member, 1);
	}
	r2i_printf(r, "};\n\n");
}

void r2i_make_class (R2i *r, ASTNode *n)
{
	Symbol *sym = r->ir->symtab->symbols + n->sym;
	const char *name = r->type_syms[sym->type];

	r2i_printf(r, "%s class %s {\n",
			(n->flags & NODE_FLAG_EXPORT) ? "export" : "pub", name);
	for (uint32_t m = n->child; m != NO_NODE; m = r->ir->ast->nodes[m].next_bro) {
		ASTNode *member = r->ir->ast->nodes + m;

		if (member->type == NODE_FIELD_DEC) r2i_make_field(r, member, 1);
		else if (member->type == NODE_STRUCT_DEC) r2i_make_nested(r, member, 1);
		else if (member->type == NODE_FN_DEC
				&& !(member->flags & NODE_FLAG_PRIV)) {
			r2i_indent(r, 1);
			r2i_make_method(r, member);
		}
	}
	r2i_printf(r, "};\n\n");
}

void r2i_make_enum (R2i *r, ASTNode *n)
{
	Symbol *sym = r->ir->symtab->symbols + n->sym;
	TypeId id = sym->type;
	Type *t = types + id;

	r2i_printf(r, "%s enum %s : ",
			(n->flags & NODE_FLAG_EXPORT) ? "export" : "pub", r->type_syms[id]);
	r2i_print_type(r, n->data_type);
	r2i_printf(r, " {\n");
	for (uint16_t i = 0; i < t->variant_count; i++) {
		const EnumVariant *v = t->variants + i;

		if (t->sign)
			r2i_printf(r, "\t%.*s = %lld,\n", (int)v->len, v->name,
					(long long)(int64_t)v->value);
		else r2i_printf(r, "\t%.*s = %llu,\n", (int)v->len, v->name,
				(unsigned long long)v->value);
	}
	r2i_printf(r, "};\n\n");
}

void r2i_make_alias (R2i *r, ASTNode *n, int is_newtype)
{
	Symbol *sym = r->ir->symtab->symbols + n->sym;

	r2i_printf(r, "%s %s %s = ",
			(n->flags & NODE_FLAG_EXPORT) ? "export" : "pub",
			is_newtype ? "newtype" : "type", r->type_syms[sym->type]);
	r2i_print_type(r, n->data_type);
	r2i_printf(r, ";\n\n");
}
