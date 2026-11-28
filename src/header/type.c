#include "header/internal.h"

#include <string.h>

#include "mangle/mangle.h"
#include "parser/operators.h"

static void h_collect_type (CHeader *h, TypeId type);
static void h_collect_fn (CHeader *h, TypeId type);
static void h_print_at (CHeader *h, TypeId type, int pointee);

static void h_print_builtin (CHeader *h, TypeId type, int pointee)
{
	switch (type)
	{
	case TYPE_VOID: h_printf(h, "void"); return;
	case TYPE_ANY: h_printf(h, "r2_any"); h->any_used = 1; return;
	case TYPE_i8: h_printf(h, pointee ? "char" : "int8_t"); return;
	case TYPE_u8: h_printf(h, "uint8_t"); return;
	case TYPE_i16: h_printf(h, "int16_t"); return;
	case TYPE_u16: h_printf(h, "uint16_t"); return;
	case TYPE_i32: h_printf(h, "int32_t"); return;
	case TYPE_u32: h_printf(h, "uint32_t"); return;
	case TYPE_i64: h_printf(h, "int64_t"); return;
	case TYPE_u64: h_printf(h, "uint64_t"); return;
	case TYPE_UNTYPED_INT: case TYPE_UNTYPED_CHAR:
		h_printf(h, "int64_t"); return;
	case TYPE_UNTYPED_UINT: h_printf(h, "uint64_t"); return;
	case TYPE_ERROR: h_printf(h, "void"); return;
	default: h_printf(h, "%s", types[type].name); return;
	}
}

static void h_print_array (CHeader *h, TypeId type)
{
	uint32_t counts[16];
	int rank = 0;

	while (type_kind(type) == TK_ARRAY) {
		if (rank < 16) counts[rank++] = types[type].count;
		type = types[type].base;
	}
	h_print_at(h, type, 1);
	for (int i = 0; i < rank; i++)
		h_printf(h, "[%u]", counts[i]);
}

static uint16_t h_fn_index (CHeader *h, TypeId type)
{
	if (type >= h->type_total || h->fn_index[type] == 0xFFFF) return 0;
	return h->fn_index[type];
}

static uint16_t h_slice_index (CHeader *h, TypeId type)
{
	if (type >= h->type_total || h->slice_index[type] == 0xFFFF) return 0;
	return h->slice_index[type];
}

void h_print_named_type (CHeader *h, TypeId type, const char *name, int len)
{
	uint32_t counts[16];
	int rank = 0;
	int prefix = 0;

	while (type_kind(type) == TK_ARRAY) {
		if (type_is_const(type)) prefix = 1;
		if (rank < 16) counts[rank++] = types[type].count;
		type = types[type].base;
	}
	if (type_is_const(type) && type_kind(type) != TK_PTR) {
		prefix = 1;
		type = type_unconst_of(type);
	}
	if (prefix) h_printf(h, "const ");
	h_print_at(h, type, rank > 0);
	h_printf(h, " %.*s", len, name);
	for (int i = 0; i < rank; i++)
		h_printf(h, "[%u]", counts[i]);
}

static void h_print_at (CHeader *h, TypeId type, int pointee)
{
	if (type != TYPE_ERROR && type_is_const(type)) {
		if (type_kind(type) == TK_PTR) {
			h_print_at(h, type_unconst_of(type), pointee);
			h_printf(h, " const");
		} else {
			h_printf(h, "const ");
			h_print_at(h, type_unconst_of(type), pointee);
		}
		return;
	}
	if (type == TYPE_ERROR || type < TYPE_COUNT) {
		h_print_builtin(h, type, pointee);
		return;
	}
	switch (type_kind(type))
	{
	case TK_PTR:
		h_print_at(h, types[type].base, 1);
		if (type_kind(types[type].base) != TK_FN) h_printf(h, " *");
		return;
	case TK_ARRAY:
		h_print_array(h, type);
		return;
	case TK_SLICE:
		h_printf(h, "r2_slice_%u", h_slice_index(h, type));
		return;
	case TK_FN:
		h_printf(h, "r2_fn_%u", h_fn_index(h, type));
		return;
	default:
		if (h->type_names[type]) h_printf(h, "%s", h->type_names[type]);
		else h_printf(h, "%s", types[type].name);
		return;
	}
}

void h_print_type (CHeader *h, TypeId type)
{
	h_print_at(h, type, 0);
}

void h_print_pointee (CHeader *h, TypeId type)
{
	h_print_at(h, type, 1);
}

static void h_collect_type (CHeader *h, TypeId type)
{
	if (type == TYPE_ERROR || type >= type_count) return;
	if (type_is_const(type)) {
		h_collect_type(h, type_unconst_of(type));
		return;
	}
	if (h->type_used[type]) return;
	h->type_used[type] = 1;
	if (type == TYPE_ANY) h->any_used = 1;
	switch (type_kind(type))
	{
	case TK_PTR: case TK_ARRAY:
		h_collect_type(h, types[type].base);
		return;
	case TK_SLICE:
		h_collect_type(h, types[type].base);
		if (h->slice_index[type] == 0xFFFF) {
			h->slice_index[type] = h->slice_count;
			h->slice_types[h->slice_count++] = type;
		}
		return;
	case TK_FN:
		h_collect_fn(h, type);
		return;
	case TK_STRUCT: case TK_CLASS:
		for (uint16_t i = 0; i < types[type].field_count; i++)
			h_collect_type(h, types[type].fields[i].type);
		return;
	case TK_ALIAS: case TK_ENUM: case TK_NEWTYPE:
		h_collect_type(h, types[type].base);
		return;
	default: return;
	}
}

static void h_collect_fn (CHeader *h, TypeId type)
{
	Type *fn = types + type;

	for (uint16_t i = 0; i < fn->param_count; i++)
		h_collect_type(h, fn->params[i]);
	for (uint16_t i = 0; i < fn->ret_count; i++)
		h_collect_type(h, fn->rets[i]);
	if (h->fn_index[type] == 0xFFFF) {
		h->fn_index[type] = h->fn_count;
		h->fn_types[h->fn_count++] = type;
	}
}

static void h_collect_fields (CHeader *h, uint32_t first)
{
	AST *ast = h->ir->ast;

	for (uint32_t m = first; m != NO_NODE; m = ast->nodes[m].next_bro) {
		ASTNode *n = ast->nodes + m;

		if (n->type == NODE_FIELD_DEC) h_collect_type(h, n->data_type);
		else if (n->type == NODE_STRUCT_DEC) h_collect_fields(h, n->child);
	}
}

static void h_collect_sig (CHeader *h, ASTNode *n)
{
	AST *ast = h->ir->ast;
	uint32_t args = n->child;
	uint32_t ret = ast->nodes[args].next_bro;

	for (uint32_t p = ast->nodes[args].child; p != NO_NODE;
			p = ast->nodes[p].next_bro)
		h_collect_type(h, ast->nodes[p].data_type);
	h_collect_type(h, ast->nodes[ret].data_type);
	for (uint32_t item = ast->nodes[ret].child; item != NO_NODE;
			item = ast->nodes[item].next_bro)
		h_collect_type(h, ast->nodes[item].data_type);
}

static void h_collect_decl (CHeader *h, ASTNode *n, uint32_t idx)
{
	switch (n->type)
	{
	case NODE_FN_DEC:
		h_collect_sig(h, n);
		return;
	case NODE_VAR_DEC: case NODE_ARR_DEC:
		h_collect_type(h, n->data_type);
		return;
	case NODE_STRUCT_DEC:
		h_collect_fields(h, n->child);
		h_collect_type(h, n->data_type);
		return;
	case NODE_CLASS_DEC: {
		AST *ast = h->ir->ast;

		h_collect_fields(h, n->child);
		h_collect_type(h, n->data_type);
		for (uint32_t m = n->child; m != NO_NODE; m = ast->nodes[m].next_bro)
			if (ast->nodes[m].type == NODE_FN_DEC
					&& !(ast->nodes[m].flags & NODE_FLAG_PRIV))
				h_collect_sig(h, ast->nodes + m);
		return;
	}
	case NODE_ENUM_DEC: case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		h_collect_type(h, n->data_type);
		return;
	default:
		(void)idx;
		return;
	}
}

static void h_register_type (CHeader *h, ASTNode *n, uint32_t idx)
{
	uint32_t sym = n->sym;
	TypeId id;

	if (sym == NO_SYMBOL) return;
	id = h->ir->symtab->symbols[sym].type;
	if (id == TYPE_ERROR || id >= type_count || !n->str) return;
	char *name = arena_alloc(h->ir->arena, (size_t)n->len + 1);

	memcpy(name, n->str, n->len);
	name[n->len] = '\0';
	h->type_names[id] = name;
	h->decl_nodes[id] = (uint16_t)idx;
	h_collect_type(h, id);
}

static void h_register_imported (CHeader *h)
{
	AST *ast = h->ir->ast;

	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		ASTNode *n = ast->nodes + idx;
		TypeId id;

		if (!(n->flags & NODE_FLAG_IMPORTED) || n->sym == NO_SYMBOL) continue;
		id = h->ir->symtab->symbols[n->sym].type;
		if (id == TYPE_ERROR || id >= type_count || !h->type_used[id]) continue;
		if (h->decl_nodes[id] != 0xFFFF) continue;
		h->type_names[id] = types[id].name;
		h->decl_nodes[id] = (uint16_t)idx;
		h_collect_type(h, id);
		if (n->type == NODE_CLASS_DEC) h_collect_decl(h, n, idx);
	}
}

void h_collect_all (CHeader *h)
{
	AST *ast = h->ir->ast;

	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		ASTNode *n = ast->nodes + idx;

		if (!(n->flags & NODE_FLAG_EXPORT)) continue;
		switch (n->type)
		{
		case NODE_STRUCT_DEC: case NODE_ENUM_DEC:
		case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
			h_register_type(h, n, idx);
			break;
		case NODE_CLASS_DEC:
			h_register_type(h, n, idx);
			h_collect_decl(h, n, idx);
			break;
		default:
			h_collect_decl(h, n, idx);
			break;
		}
	}
	h_register_imported(h);
}

static void h_make_slices (CHeader *h)
{
	for (uint16_t i = 0; i < h->slice_count; i++) {
		TypeId type = h->slice_types[i];
		TypeId leaf = type;
		int rank = type_slice_rank(type);

		while (type_kind(leaf) == TK_SLICE) leaf = types[leaf].base;
		h_printf(h, "typedef struct {\n\t");
		h_print_pointee(h, leaf);
		h_printf(h, " *data;\n\tint64_t len[%d];\n} r2_slice_%u;\n\n",
				rank, (unsigned)i);
	}
}

static void h_make_fn_typedefs (CHeader *h)
{
	for (uint16_t i = 0; i < h->fn_count; i++) {
		Type *fn = types + h->fn_types[i];

		h_printf(h, "typedef ");
		if (fn->ret_count) h_print_type(h, fn->rets[0]);
		else h_printf(h, "void");
		h_printf(h, " (*r2_fn_%u)(", (unsigned)i);
		for (uint16_t k = 0; k < fn->param_count; k++) {
			if (k) h_printf(h, ", ");
			h_print_type(h, fn->params[k]);
		}
		if (fn->fn_variadic)
			h_printf(h, "%s...", fn->param_count ? ", " : "");
		h_printf(h, ");\n");
	}
	if (h->fn_count) h_printf(h, "\n");
}

static void h_make_any (CHeader *h)
{
	if (!h->any_used) return;
	h_printf(h,
		"typedef struct {\n"
		"\tconst char *tag;\n"
		"\tint64_t value;\n"
		"} r2_any;\n\n");
}

typedef struct HFieldState {
	TypeId st;
	uint32_t index;
	uint32_t last_off;
	uint32_t bit_pos;
	int packed;
} HFieldState;

static void h_make_field (CHeader *h, ASTNode *n, HFieldState *s, int depth)
{
	StructField *f = types[s->st].fields + s->index++;

	h_indent(h, depth);
	if (!(n->flags & NODE_FLAG_BITFIELD)) {
		h_print_named_type(h, n->data_type, n->str, (int)n->len);
		h_printf(h, ";\n");
		s->bit_pos = 0;
		return;
	}
	TypeId holder = f->type;

	if (f->offset != s->last_off) {
		s->last_off = f->offset;
		s->bit_pos = 0;
	}
	if (f->bit_off > s->bit_pos) {
		h_print_type(h, holder);
		h_printf(h, " : %u; /* padding */\n", f->bit_off - s->bit_pos);
		h_indent(h, depth);
	}
	h_print_type(h, holder);
	h_printf(h, " %.*s : %u; /* bitfield layout is compiler-dependent */\n",
			(int)n->len, n->str, (unsigned)n->sym);
	s->bit_pos = f->bit_off + (uint32_t)n->sym;
}

static void h_make_members (CHeader *h, uint32_t first, int depth, HFieldState *s)
{
	AST *ast = h->ir->ast;

	for (uint32_t m = first; m != NO_NODE; m = ast->nodes[m].next_bro) {
		ASTNode *n = ast->nodes + m;

		if (n->type == NODE_FIELD_DEC) {
			h_make_field(h, n, s, depth);
			continue;
		}
		if (n->type != NODE_STRUCT_DEC) continue;
		h_indent(h, depth);
		h_printf(h, "%s {\n", (n->flags & NODE_FLAG_UNION) ? "union" : "struct");
		h_make_members(h, n->child, depth + 1, s);
		h_indent(h, depth);
		h_printf(h, "}%s;\n", s->packed ? " R2_PACKED" : "");
	}
}

static void h_make_asserts (CHeader *h, TypeId id)
{
	Type *t = types + id;
	const char *name = h->type_names[id];

	h_printf(h,
		"#if defined(__cplusplus) || (defined(__STDC_VERSION__) "
		"&& __STDC_VERSION__ >= 201112L)\n");
	h_printf(h,
		"#if defined(__cplusplus)\n"
		"static_assert(sizeof(%s) == %u, \"%s layout\");\n"
		"#else\n"
		"_Static_assert(sizeof(%s) == %u, \"%s layout\");\n"
		"#endif\n",
		name, t->size, name, name, t->size, name);
	for (uint16_t i = 0; i < t->field_count; i++) {
		StructField *f = t->fields + i;

		if (t->is_union || f->bits) continue;
		h_printf(h,
			"#if defined(__cplusplus)\n"
			"static_assert(offsetof(%s, %.*s) == %u, \"offset\");\n"
			"#else\n"
			"_Static_assert(offsetof(%s, %.*s) == %u, \"offset\");\n"
			"#endif\n",
			name, (int)f->len, f->name, f->offset,
			name, (int)f->len, f->name, f->offset);
	}
	h_printf(h, "#endif\n");
}

static const char *h_method_link (CHeader *h, ASTNode *fn, TypeId id)
{
	Symbol *sym = h->ir->symtab->symbols + fn->sym;
	Type *cls = types + id;

	if (sym->link) return sym->link;
	if (fn->op != OP_NONE)
		return mangle_class_special(h->ir->arena, h->ir->mod, cls->name,
				cls->len, op_canon(fn->op));
	if (fn->flags & NODE_FLAG_INIT)
		return mangle_class_special(h->ir->arena, h->ir->mod, cls->name,
				cls->len, "init");
	if (fn->flags & NODE_FLAG_DESTROY)
		return mangle_class_special(h->ir->arena, h->ir->mod, cls->name,
				cls->len, "destroy");

	MangleName name = {
		.kind = "m", .mod = h->ir->mod, .a = cls->name, .alen = cls->len,
		.b = fn->str, .blen = fn->len
	};

	return mangle_symbol(h->ir->arena, &name);
}

static uint32_t h_self_param (CHeader *h, ASTNode *fn)
{
	uint32_t p = h->ir->ast->nodes[fn->child].child;

	if (p != NO_NODE && (h->ir->ast->nodes[p].flags & NODE_FLAG_SELF))
		return p;
	return NO_NODE;
}

static int h_cpp_bool_ret (ASTNode *fn)
{
	return fn->op == OP_EQ || fn->op == OP_NE || fn->op == OP_LT
			|| fn->op == OP_LE || fn->op == OP_GT || fn->op == OP_GE
			|| fn->op == OP_NOT;
}

static const char *h_cpp_operator (uint8_t op)
{
	switch (op)
	{
	case OP_EQ: return "==";
	case OP_NE: return "!=";
	case OP_LT: return "<";
	case OP_LE: return "<=";
	case OP_GT: return ">";
	case OP_GE: return ">=";
	case OP_ADD: return "+";
	case OP_SUB: return "-";
	case OP_MUL: return "*";
	case OP_DIV: return "/";
	case OP_MOD: return "%";
	case OP_AND: return "&";
	case OP_OR: return "|";
	case OP_XOR: return "^";
	case OP_SHL: return "<<";
	case OP_SHR: return ">>";
	case OP_NEG: return "-";
	case OP_NOT: return "!";
	case OP_BNOT: return "~";
	case OP_ASSIGN: return "=";
	case OP_ADD_ASSIGN: return "+=";
	case OP_SUB_ASSIGN: return "-=";
	case OP_MUL_ASSIGN: return "*=";
	case OP_DIV_ASSIGN: return "/=";
	case OP_MOD_ASSIGN: return "%=";
	case OP_AND_ASSIGN: return "&=";
	case OP_OR_ASSIGN: return "|=";
	case OP_XOR_ASSIGN: return "^=";
	case OP_SHL_ASSIGN: return "<<=";
	case OP_SHR_ASSIGN: return ">>=";
	case OP_INC: return "++";
	case OP_DEC: return "--";
	default: return "";
	}
}

static uint32_t h_index_count (CHeader *h, ASTNode *fn)
{
	AST *ast = h->ir->ast;
	uint32_t count = 0;

	for (uint32_t p = ast->nodes[fn->child].child; p != NO_NODE;
			p = ast->nodes[p].next_bro)
		if (!(ast->nodes[p].flags & NODE_FLAG_SELF)) count++;
	return count;
}

static void h_cpp_name (CHeader *h, ASTNode *fn, TypeId id)
{
	const char *name = h->type_names[id];

	if (fn->op == OP_INDEX && h_index_count(h, fn) == 1) h_printf(h, "operator[]");
	else if (fn->op == OP_INDEX) h_printf(h, "index");
	else if (fn->op != OP_NONE)
		h_printf(h, "operator%s", h_cpp_operator(fn->op));
	else if (fn->flags & NODE_FLAG_INIT) h_printf(h, "%s", name);
	else h_printf(h, "%.*s", (int)fn->len, fn->str);
}

static void h_cpp_args (CHeader *h, uint32_t args)
{
	AST *ast = h->ir->ast;
	int first = 1;

	for (uint32_t p = ast->nodes[args].child; p != NO_NODE;
			p = ast->nodes[p].next_bro) {
		if (ast->nodes[p].flags & NODE_FLAG_SELF) continue;
		if (!first) h_printf(h, ", ");
		first = 0;
		h_make_param(h, ast->nodes + p, 1);
	}
	if (first) h_printf(h, "void");
}

static void h_cpp_call_args (CHeader *h, ASTNode *fn, TypeId id)
{
	AST *ast = h->ir->ast;
	uint32_t args = fn->child;
	int first = 1;

	if (h_self_param(h, fn) != NO_NODE) {
		h_printf(h, "const_cast<%s *>(this)", h->type_names[id]);
		first = 0;
	}
	for (uint32_t p = ast->nodes[args].child; p != NO_NODE;
			p = ast->nodes[p].next_bro) {
		ASTNode *n = ast->nodes + p;

		if (n->flags & NODE_FLAG_SELF) continue;
		if (!first) h_printf(h, ", ");
		first = 0;
		h_printf(h, "%.*s", (int)n->len, n->str);
	}
}

static void h_cpp_member (CHeader *h, ASTNode *fn, TypeId id)
{
	AST *ast = h->ir->ast;
	uint32_t args = fn->child;
	uint32_t ret = ast->nodes[args].next_bro;
	uint32_t self = h_self_param(h, fn);
	int special = (fn->flags & NODE_FLAG_INIT) != 0;

	h_printf(h, "\t");
	if (!special) {
		if (self == NO_NODE) h_printf(h, "static ");
		if (h_cpp_bool_ret(fn)) h_printf(h, "bool ");
		else {
			h_print_type(h, ast->nodes[ret].data_type);
			h_printf(h, " ");
		}
	}
	h_cpp_name(h, fn, id);
	h_printf(h, " (");
	h_cpp_args(h, args);
	h_printf(h, ")");
	if (!special && self != NO_NODE) h_printf(h, " const");
	h_printf(h, ";\n");
}

static void h_cpp_inline (CHeader *h, ASTNode *fn, TypeId id)
{
	AST *ast = h->ir->ast;
	uint32_t args = fn->child;
	uint32_t ret = ast->nodes[args].next_bro;
	uint32_t self = h_self_param(h, fn);
	int special = (fn->flags & NODE_FLAG_INIT) != 0;
	int void_ret = !special && type_unconst_of(ast->nodes[ret].data_type) == TYPE_VOID;

	h_printf(h, "inline ");
	if (!special) {
		if (h_cpp_bool_ret(fn)) h_printf(h, "bool ");
		else if (void_ret) h_printf(h, "void ");
		else {
			h_print_type(h, ast->nodes[ret].data_type);
			h_printf(h, " ");
		}
	}
	h_printf(h, "%s::", h->type_names[id]);
	h_cpp_name(h, fn, id);
	h_printf(h, " (");
	h_cpp_args(h, args);
	h_printf(h, ")");
	if (!special && self != NO_NODE) h_printf(h, " const");
	h_printf(h, " { %s%s (",
			(void_ret || special) ? "" : "return ", h_method_link(h, fn, id));
	h_cpp_call_args(h, fn, id);
	h_printf(h, "); }\n");
}

static void h_c_proto (CHeader *h, ASTNode *fn, TypeId id)
{
	AST *ast = h->ir->ast;
	uint32_t args = fn->child;
	uint32_t ret = ast->nodes[args].next_bro;

	h_printf(h, "R2_API ");
	if (fn->flags & (NODE_FLAG_INIT | NODE_FLAG_DESTROY)) h_printf(h, "void");
	else h_print_type(h, ast->nodes[ret].data_type);
	h_printf(h, " %s (", h_method_link(h, fn, id));
	h_make_params(h, args);
	h_printf(h, ");\n");
}

static void h_make_class (CHeader *h, TypeId id)
{
	AST *ast = h->ir->ast;
	ASTNode *n = ast->nodes + h->decl_nodes[id];
	const char *name = h->type_names[id];
	HFieldState cpp = { .st = id, .index = 0, .last_off = 0, .bit_pos = 0, .packed = 0 };
	HFieldState c = cpp;

	h_printf(h, "#ifdef __cplusplus\nclass %s {\npublic:\n", name);
	h_make_members(h, n->child, 1, &cpp);
	for (uint32_t m = n->child; m != NO_NODE; m = ast->nodes[m].next_bro) {
		ASTNode *fn = ast->nodes + m;

		if (fn->type == NODE_FN_DEC && !(fn->flags & NODE_FLAG_PRIV))
			h_cpp_member(h, fn, id);
	}
	h_printf(h, "};\n#else\nstruct %s {\n", name);
	h_make_members(h, n->child, 1, &c);
	h_printf(h, "};\n#endif\n\n");

	for (uint32_t m = n->child; m != NO_NODE; m = ast->nodes[m].next_bro)
		if (ast->nodes[m].type == NODE_FN_DEC
				&& !(ast->nodes[m].flags & NODE_FLAG_PRIV))
			h_c_proto(h, ast->nodes + m, id);
	h_printf(h, "#ifdef __cplusplus\n");
	for (uint32_t m = n->child; m != NO_NODE; m = ast->nodes[m].next_bro)
		if (ast->nodes[m].type == NODE_FN_DEC
				&& !(ast->nodes[m].flags & NODE_FLAG_PRIV))
			h_cpp_inline(h, ast->nodes + m, id);
	h_printf(h, "#endif\n");
	h_make_asserts(h, id);
	h_printf(h, "\n");
}

static void h_make_struct (CHeader *h, TypeId id)
{
	ASTNode *n = h->ir->ast->nodes + h->decl_nodes[id];
	const char *name = h->type_names[id];
	int packed = (n->flags & NODE_FLAG_PACKED) != 0;

	if (packed)
		h_printf(h,
			"#if !defined(_MSC_VER) && !defined(__GNUC__) && !defined(__clang__)\n"
			"#error \"r2p: cannot express packed layout with this compiler\"\n"
			"#endif\n"
			"R2_PACK_PUSH\n");
	h_printf(h, "%s %s {\n",
			(n->flags & NODE_FLAG_UNION) ? "union" : "struct", name);
	HFieldState state = { .st = id, .index = 0, .last_off = 0, .bit_pos = 0, .packed = packed };

	h_make_members(h, n->child, 1, &state);
	h_printf(h, "}%s;\n", packed ? " R2_PACKED" : "");
	if (packed) h_printf(h, "R2_PACK_POP\n");
	h_make_asserts(h, id);
	h_printf(h, "\n");
}

static void h_make_variants (CHeader *h, TypeId id, const char *name)
{
	Type *t = types + id;

	for (uint16_t i = 0; i < t->variant_count; i++) {
		const EnumVariant *v = t->variants + i;

		h_printf(h, "\t%s_%.*s = ", name, (int)v->len, v->name);
		if (t->sign) h_printf(h, "%lld,\n", (long long)(int64_t)v->value);
		else h_printf(h, "%llu,\n", (unsigned long long)v->value);
	}
}

static void h_make_enum (CHeader *h, TypeId id)
{
	ASTNode *n = h->ir->ast->nodes + h->decl_nodes[id];
	const char *name = h->type_names[id];

	h_printf(h, "#ifdef __cplusplus\nenum %s : ", name);
	h_print_type(h, n->data_type);
	h_printf(h, " {\n");
	h_make_variants(h, id, name);
	h_printf(h, "};\n#else\ntypedef ");
	h_print_type(h, n->data_type);
	h_printf(h, " %s;\nenum {\n", name);
	h_make_variants(h, id, name);
	h_printf(h, "};\n#endif\n\n");
}

static void h_make_alias (CHeader *h, TypeId id)
{
	ASTNode *n = h->ir->ast->nodes + h->decl_nodes[id];

	h_printf(h, "typedef ");
	h_print_named_type(h, n->data_type, h->type_names[id],
			(int)strlen(h->type_names[id]));
	h_printf(h, ";\n\n");
}

static void h_make_nominal (CHeader *h, TypeId id);

static void h_make_deps (CHeader *h, TypeId type)
{
	while (type < type_count) {
		TypeKind kind = type_kind(type);

		if (kind != TK_PTR && kind != TK_ARRAY && kind != TK_SLICE
				&& kind != TK_ALIAS && kind != TK_NEWTYPE)
			break;
		type = types[type].base;
	}
	if (type >= type_count) return;
	if (type_is_struct_like(type)) h_make_nominal(h, type);
	if (type_kind(type) == TK_FN) {
		Type *fn = types + type;

		for (uint16_t i = 0; i < fn->param_count; i++)
			h_make_nominal(h, fn->params[i]);
		for (uint16_t i = 0; i < fn->ret_count; i++)
			h_make_nominal(h, fn->rets[i]);
	}
}

static void h_make_nominal (CHeader *h, TypeId id)
{
	if (!h->type_used[id] || h->type_done[id]) return;
	h->type_done[id] = 1;
	if (h->decl_nodes[id] == 0xFFFF) return;
	ASTNode *n = h->ir->ast->nodes + h->decl_nodes[id];
	switch (n->type)
	{
	case NODE_STRUCT_DEC: {
		for (uint16_t i = 0; i < types[id].field_count; i++)
			h_make_deps(h, types[id].fields[i].type);
		h_make_struct(h, id);
		return;
	}
	case NODE_CLASS_DEC: {
		for (uint16_t i = 0; i < types[id].field_count; i++)
			h_make_deps(h, types[id].fields[i].type);
		h_make_class(h, id);
		return;
	}
	case NODE_ENUM_DEC:
		h_make_enum(h, id);
		return;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		h_make_deps(h, n->data_type);
		h_make_alias(h, id);
		return;
	default: return;
	}
}

void h_make_types (CHeader *h)
{
	size_t mark = h->len;

	for (uint16_t id = TYPE_COUNT; id < h->type_total; id++) {
		uint16_t node = h->decl_nodes[id];

		if (!h->type_used[id] || node == 0xFFFF) continue;
		ASTNode *n = h->ir->ast->nodes + node;
		if (n->type == NODE_STRUCT_DEC || n->type == NODE_CLASS_DEC)
			h_printf(h, "typedef %s %s %s;\n",
					(n->flags & NODE_FLAG_UNION) ? "union" : "struct",
					h->type_names[id], h->type_names[id]);
		else if (n->type == NODE_ENUM_DEC) {
			h_printf(h, "#ifdef __cplusplus\nenum %s : ", h->type_names[id]);
			h_print_type(h, n->data_type);
			h_printf(h, ";\n#else\ntypedef ");
			h_print_type(h, n->data_type);
			h_printf(h, " %s;\n#endif\n", h->type_names[id]);
		}
	}
	h_make_any(h);
	h_make_slices(h);
	h_make_fn_typedefs(h);
	if (h->len > mark) h_printf(h, "\n");
	mark = h->len;
	for (uint16_t id = TYPE_COUNT; id < h->type_total; id++)
		h_make_nominal(h, id);
	if (h->len > mark) h_printf(h, "\n");
}
