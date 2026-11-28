#include "sema/common.h"

#include <string.h>

static void decl_enum (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->len == 4 && memcmp(n->str, "main", 4) == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'main' must be a function");
		return;
	}
	if (symtab_find(&s->table, n->str, n->len) != NO_SYMBOL) {
		sema_declare(s, idx, SYMBOL_ENUM, TYPE_ERROR);
		return;
	}

	TypeId id = add_type(n->str, n->len, TK_ENUM, TYPE_u32);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return;
	}
	sema_declare(s, idx, SYMBOL_ENUM, id);
}

static void decl_types (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_ENUM_DEC) decl_enum(s, idx);
		else if (n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC)
			sema_decl_type(s, idx);
		else if (n->type == NODE_STRUCT_DEC || n->type == NODE_CLASS_DEC)
			sema_decl_struct(s, idx);
		idx = n->next_bro;
	}
}

static void resolve_enum_base (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym;
	TypeId base;
	uint32_t loc;

	if (n->sym == NO_SYMBOL) return;
	sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR) return;

	loc = n->type_node != NO_TYPE_NODE
			? (uint32_t)(n - s->ast->nodes) + n->type_node : idx;
	base = resolve_type_node(s, n);
	if (base == TYPE_ERROR) {
		sym->type = TYPE_ERROR;
		return;
	}
	if (type_kind(base) != TK_INT) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + loc),
				"enum underlying type must be an integer type");
		sym->type = TYPE_ERROR;
		return;
	}
	types[sym->type].base = base;
	types[sym->type].size = types[base].size;
	types[sym->type].sign = types[base].sign;
}

static void resolve_enum_bases (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		if (s->ast->nodes[idx].type == NODE_ENUM_DEC)
			resolve_enum_base(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static uint64_t enum_max (TypeId id)
{
	unsigned bits = types[id].size * 8u;

	if (types[id].sign)
		return bits == 64 ? INT64_MAX : ((uint64_t)1 << (bits - 1)) - 1;
	return bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
}

static int enum_next_value (Sema *s, ASTNode *n, TypeId id, uint64_t prev,
		uint64_t *out)
{
	uint64_t max = enum_max(id);
	int full = types[id].sign ? (int64_t)prev >= (int64_t)max : prev >= max;

	if (full) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enum variant value overflows the underlying type %s",
				types[types[id].base].name);
		*out = 0;
		return 0;
	}
	*out = prev + 1;
	return 1;
}

static int variant_duplicate (Sema *s, const EnumVariant *vars, uint16_t count,
		ASTNode *n)
{
	for (uint16_t i = 0; i < count; i++)
		if (vars[i].len == n->len && memcmp(vars[i].name, n->str, n->len) == 0) {
			error_report(s->err, ERR_ERROR, node_loc(n),
					"duplicate enum variant '%.*s'", (int)n->len, n->str);
			return 1;
		}
	return 0;
}

static void bind_enum (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (n->sym == NO_SYMBOL) return;
	Symbol *sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR) return;

	uint16_t count = 0;
	for (uint32_t var = n->child; var != NO_NODE; var = s->ast->nodes[var].next_bro)
		count++;

	if (count == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"enum '%.*s' must have at least one variant",
				(int)n->len, n->str);
		return;
	}

	EnumVariant *vars = arena_alloc(s->arena, sizeof(EnumVariant) * count);
	count = 0;
	uint64_t prev = 0;

	for (uint32_t var = n->child; var != NO_NODE; var = s->ast->nodes[var].next_bro) {
		ASTNode *v = s->ast->nodes + var;
		uint64_t value = 0;
		int ok = 1;

		variant_duplicate(s, vars, count, v);
		if (v->child != NO_NODE)
			ok = enum_const_value(s, v->child, sym->type, &value);
		else if (count != 0)
			ok = enum_next_value(s, v, sym->type, prev, &value);

		vars[count].name = v->str;
		vars[count].len = v->len;
		vars[count].decl = var;
		vars[count].used = 0;
		vars[count].value = ok ? value : 0;
		prev = vars[count].value;
		count++;
	}

	bind_enum_type(sym->type, vars, count);
}

static void bind_enums (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		if (s->ast->nodes[idx].type == NODE_ENUM_DEC) bind_enum(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static void check_module_decl (Sema *s, ASTNode *n)
{
	if (s->mod_seen) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"duplicate 'module' declaration");
		return;
	}
	s->mod_seen = 1;
	if (!n->len) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"module name cannot be empty");
		return;
	}
	s->mod_name = n->str;
	s->mod_len = n->len;
}

static int valid_dep_name (const char *s, uint16_t len)
{
	if (!len) return 0;
	for (uint16_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];

		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
				|| (c >= '0' && c <= '9') || c == '_' || c == '+'
				|| c == '-' || c == '.') continue;
		return 0;
	}
	return 1;
}

static void check_depends_decl (Sema *s, ASTNode *n)
{
	if (!n->len) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"library name in 'depends' cannot be empty");
		return;
	}
	if (!valid_dep_name(n->str, n->len)) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"invalid library name '%.*s'; use only [A-Za-z0-9_+.-]",
				(int)n->len, n->str);
		return;
	}
	for (uint16_t i = 0; i < s->dep_count; i++) {
		if (s->dep_lens[i] == n->len
				&& memcmp(s->deps[i], n->str, n->len) == 0) return;
	}
	if (s->dep_count == s->dep_cap) {
		uint16_t cap = s->dep_cap ? s->dep_cap * 2 : 8;

		s->deps = arena_realloc(s->arena, s->deps,
				sizeof(char *) * s->dep_cap, sizeof(char *) * cap);
		s->dep_lens = arena_realloc(s->arena, s->dep_lens,
				sizeof(uint16_t) * s->dep_cap, sizeof(uint16_t) * cap);
		s->dep_cap = cap;
	}
	s->deps[s->dep_count] = n->str;
	s->dep_lens[s->dep_count] = n->len;
	s->dep_count++;
}

static void check_root (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC) {
			if (n->flags & NODE_FLAG_STATIC)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"static variables can only be declared inside a function");
			check_global_var_init(s, idx);
		}
		else if (n->type == NODE_FN_DEC) {
			if (n->flags & NODE_FLAG_PRIV)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"'priv' is only allowed inside a class");
			check_fn_dec(s, idx);
		}
		else if (n->type == NODE_CLASS_DEC) check_class_methods(s, idx);
		else if (n->type == NODE_EXTERN_FN_DEC) check_extern_fn(s, idx);
		else if (n->type == NODE_MODULE_DEC) check_module_decl(s, n);
		else if (n->type == NODE_DEPENDS_DEC) check_depends_decl(s, n);
		else if (n->type != NODE_ENUM_DEC && n->type != NODE_TYPE_DEC
				&& n->type != NODE_NEWTYPE_DEC && n->type != NODE_EXTERN_VAR_DEC
				&& n->type != NODE_STRUCT_DEC && n->type != NODE_CLASS_DEC
				&& n->type != NODE_IMPORT_DEC && n->type != NODE_FROM_DEC)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"only variable, function, enum and type declarations are allowed at the top level");
		idx = n->next_bro;
	}
}

static void reject_untyped_global (Sema *s, ASTNode *n)
{
	if (n->type != NODE_VAR_DEC && n->type != NODE_ARR_DEC) return;
	if (n->type_node != NO_TYPE_NODE || n->data_type != TYPE_VOID) return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"cannot infer the type of global '%.*s'; add an explicit type",
			(int)n->len, n->str);
	n->data_type = TYPE_ERROR;
}

TypeId build_fn_value (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t param = s->ast->nodes[args].child;
	uint16_t param_count = 0;
	uint16_t ret_count = (uint16_t)ast_ret_count(s->ast, ret);
	uint16_t used = 0;
	uint16_t k = 0;

	if (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED) return TYPE_ERROR;
	for (uint32_t p = param; p != NO_NODE; p = s->ast->nodes[p].next_bro) {
		if (s->ast->nodes[p].child != NO_NODE) return TYPE_ERROR;
		param_count++;
	}

	TypeId *params = arena_alloc(s->arena, sizeof(TypeId) * (param_count + 1u));
	TypeId *rets = arena_alloc(s->arena, sizeof(TypeId) * (ret_count + 1u));

	for (uint32_t p = param; p != NO_NODE; p = s->ast->nodes[p].next_bro) {
		TypeId t = type_unconst_of(s->ast->nodes[p].data_type);

		if (t == TYPE_ERROR) return TYPE_ERROR;
		params[k++] = t;
	}
	for (uint16_t i = 0; i < ret_count; i++) {
		TypeId t = type_unconst_of(s->ast->nodes[ast_ret_at(s->ast, ret, i)].data_type);

		if (t == TYPE_ERROR) return TYPE_ERROR;
		if (t == TYPE_VOID && ret_count == 1) continue;
		rets[used++] = t;
	}

	TypeId fn = type_fn_of(params, param_count, rets, used,
			(s->ast->nodes[args].flags & NODE_FLAG_VARARG) != 0);

	if (fn == TYPE_ERROR) return TYPE_ERROR;
	return type_ptr_to(fn);
}

static void decl_globals (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type == NODE_FN_DEC || n->type == NODE_EXTERN_FN_DEC) {
			uint32_t ret = s->ast->nodes[n->child].next_bro;
			resolve_fn_types(s, idx);
			uint32_t sym = sema_declare(s, idx, SYMBOL_FN, s->ast->nodes[ret].data_type);
			s->table.symbols[sym].fn_type = build_fn_value(s, idx);
		} else if (n->type == NODE_CLASS_DEC) {
			decl_class_methods(s, idx);
		} else if (n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC
				|| n->type == NODE_EXTERN_VAR_DEC) {
			if (n->child != NO_NODE) infer_array_dims(s, idx);
			resolve_type_node(s, n);
			check_storage_decl(s, n);
			check_const_init(s, n);
			reject_untyped_global(s, n);
			if (n->len == 4 && memcmp(n->str, "main", 4) == 0)
				error_report(s->err, ERR_ERROR, node_loc(n),
						"'main' must be a function");
			uint32_t sym = sema_declare(s, idx, SYMBOL_VAR, n->data_type);

			if (n->child != NO_NODE) mark_placement(s, sym, n->child);
		}

		idx = n->next_bro;
	}
}

static int is_extern_decl (Sema *s, Symbol *sym)
{
	ASTNode *n = s->ast->nodes + sym->decl;

	return sym->decl != NO_NODE
			&& (n->type == NODE_EXTERN_FN_DEC || n->type == NODE_EXTERN_VAR_DEC);
}

static void mark_exposed_type (TypeId type);

static void mark_exposed_struct (TypeId id)
{
	Type *t = types + id;

	if (t->exposed || t->layout != 2) return;
	t->exposed = 1;
	for (uint16_t i = 0; i < t->field_count; i++)
		mark_exposed_type(t->fields[i].type);
}

static void mark_exposed_type (TypeId type)
{
	type = type_unconst_of(type);
	for (int i = 0; i < TYPE_MAX; i++) {
		TypeKind kind = type_kind(type);

		if (kind != TK_PTR && kind != TK_ARRAY && kind != TK_SLICE
				&& kind != TK_ALIAS)
			break;
		type = types[type].base;
	}
	if (type_is_struct_like(type)) mark_exposed_struct(type);
}

static void mark_exposed_sig (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	uint32_t p = s->ast->nodes[args].child;

	while (p != NO_NODE) {
		mark_exposed_type(s->ast->nodes[p].data_type);
		p = s->ast->nodes[p].next_bro;
	}
	mark_exposed_type(s->ast->nodes[ret].data_type);
	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		mark_exposed_type(s->ast->nodes[item].data_type);
}

static void mark_exposed_decls (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		int exported = (n->flags & NODE_FLAG_EXPORT) != 0;

		if (n->type == NODE_EXTERN_FN_DEC || (n->type == NODE_FN_DEC && exported))
			mark_exposed_sig(s, idx);
		else if (n->type == NODE_EXTERN_VAR_DEC
				|| ((n->type == NODE_VAR_DEC || n->type == NODE_ARR_DEC) && exported))
			mark_exposed_type(n->data_type);
		else if (exported && (n->type == NODE_STRUCT_DEC
				|| n->type == NODE_CLASS_DEC || n->type == NODE_ENUM_DEC
				|| n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC))
			mark_exposed_type(n->data_type);
		idx = n->next_bro;
	}
}

static ASTNode *named_type_decl (Sema *s, TypeId id)
{
	if (type_is_struct_like(id)) {
		uint32_t decl = types[id].decl;

		if (decl < s->ast->count) return s->ast->nodes + decl;
	}
	for (uint32_t idx = s->ast->nodes[0].child; idx != NO_NODE;
			idx = s->ast->nodes[idx].next_bro) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->type != NODE_ENUM_DEC && n->type != NODE_TYPE_DEC
				&& n->type != NODE_NEWTYPE_DEC)
			continue;
		if (n->sym != NO_SYMBOL && s->table.symbols[n->sym].type == id)
			return n;
	}
	return NULL;
}

static void require_public_type (Sema *s, TypeId type, uint32_t use);
static void require_public_sig (Sema *s, uint32_t idx);

static void require_public_methods (Sema *s, uint32_t first)
{
	for (uint32_t m = first; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *n = s->ast->nodes + m;

		if (n->type == NODE_FN_DEC && !(n->flags & NODE_FLAG_PRIV))
			require_public_sig(s, m);
	}
}

static void require_public_fields (Sema *s, uint32_t first)
{
	for (uint32_t m = first; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *n = s->ast->nodes + m;

		if (n->type == NODE_STRUCT_DEC || n->type == NODE_CLASS_DEC)
			require_public_fields(s, n->child);
		else if (n->type == NODE_FIELD_DEC
				&& !(n->flags & NODE_FLAG_BITFIELD))
			require_public_type(s, n->data_type, m);
	}
}

static void require_public_type (Sema *s, TypeId type, uint32_t use)
{
	type = type_unconst_of(type);
	if (type == TYPE_ERROR || type < TYPE_COUNT) return;

	switch (type_kind(type))
	{
	case TK_PTR: case TK_ARRAY: case TK_SLICE:
		require_public_type(s, types[type].base, use);
		return;
	case TK_FN:
		for (uint16_t i = 0; i < types[type].param_count; i++)
			require_public_type(s, types[type].params[i], use);
		for (uint16_t i = 0; i < types[type].ret_count; i++)
			require_public_type(s, types[type].rets[i], use);
		return;
	case TK_STRUCT: case TK_CLASS: case TK_ENUM: case TK_ALIAS: case TK_NEWTYPE: {
		ASTNode *decl = named_type_decl(s, type);

		if (decl && !(decl->flags & NODE_FLAG_PUB))
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
					"public declaration exposes non-public type '%.*s'",
					(int)types[type].len, types[type].name);
		return;
	}
	default:
		return;
	}
}

static void require_public_sig (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;

	for (uint32_t p = s->ast->nodes[args].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro)
		require_public_type(s, s->ast->nodes[p].data_type, p);
	require_public_type(s, s->ast->nodes[ret].data_type, ret);
	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		require_public_type(s, s->ast->nodes[item].data_type, item);
}

static void require_public_decl (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	switch (n->type)
	{
	case NODE_FN_DEC: case NODE_EXTERN_FN_DEC:
		require_public_sig(s, idx);
		return;
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_EXTERN_VAR_DEC:
		require_public_type(s, n->data_type, idx);
		return;
	case NODE_STRUCT_DEC:
		require_public_fields(s, n->child);
		return;
	case NODE_CLASS_DEC:
		require_public_fields(s, n->child);
		require_public_methods(s, n->child);
		return;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		require_public_type(s, n->data_type, idx);
		return;
	default:
		return;
	}
}

static void check_public_types (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		uint16_t flags = s->ast->nodes[idx].flags;

		if ((flags & NODE_FLAG_PUB) && !(flags & NODE_FLAG_EXPORT))
			require_public_decl(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static int c_keyword (const char *name, uint16_t len)
{
	static const char *words[] = {
		"auto", "break", "case", "char", "const", "continue", "default", "do",
		"double", "else", "enum", "extern", "float", "for", "goto", "if",
		"inline", "int", "long", "register", "restrict", "return", "short",
		"signed", "sizeof", "static", "struct", "switch", "typedef", "union",
		"unsigned", "void", "volatile", "while", "_Bool", "_Complex",
		"_Imaginary", "_Alignas", "_Alignof", "_Atomic", "_Generic",
		"_Noreturn", "_Static_assert", "_Thread_local", "alignas", "alignof",
		"and", "and_eq", "asm", "bitand", "bitor", "bool", "catch", "class",
		"compl", "concept", "consteval", "constexpr", "constinit", "const_cast",
		"co_await", "co_return", "co_yield", "decltype", "delete",
		"dynamic_cast", "explicit", "export", "false", "friend", "mutable",
		"namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator",
		"or", "or_eq", "private", "protected", "public", "reinterpret_cast",
		"requires", "static_assert", "static_cast", "template", "this",
		"thread_local", "throw", "true", "try", "typeid", "typename", "using",
		"virtual", "wchar_t", "xor", "xor_eq"
	};
	for (uint32_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
		if (strlen(words[i]) == len && memcmp(words[i], name, len) == 0)
			return 1;
	return 0;
}

static inline void check_export_name (Sema *s, ASTNode *n)
{
	if (n->str && c_keyword(n->str, n->len))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"exported name '%.*s' is a C/C++ keyword; rename it",
				(int)n->len, n->str);
	if (n->str && n->len >= 3 && ((n->str[0] == 'R' && n->str[1] == '2'
					&& n->str[2] == '_') || (n->str[0] == 'r' && n->str[1] == '2'
					&& n->str[2] == '_')))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"exported name '%.*s' is reserved for generated headers",
				(int)n->len, n->str);
}

static void check_export_type (Sema *s, TypeId type, uint32_t use);

static int export_abi_bad (TypeId t)
{
	return type_is_struct_like(t) || type_kind(t) == TK_SLICE
			|| type_kind(t) == TK_ARRAY;
}

static void abi_error (Sema *s, uint32_t use, const char *what)
{
	error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
			"exported function pointer type has a %s ABI that cannot be represented in C",
			what);
}

static void check_export_fn_type (Sema *s, Type *fn, uint32_t use)
{
	uint16_t i;

	if (fn->ret_count > 1 || (fn->ret_count && export_abi_bad(fn->rets[0]))) {
		abi_error(s, use, "return");
		return;
	}
	for (i = 0; i < fn->param_count; i++) {
		if (export_abi_bad(fn->params[i])) {
			abi_error(s, use, "parameter");
			return;
		}
		check_export_type(s, fn->params[i], use);
	}
	for (i = 0; i < fn->ret_count; i++)
		check_export_type(s, fn->rets[i], use);
}

static void check_export_type (Sema *s, TypeId type, uint32_t use)
{
	type = type_unconst_of(type);
	if (type == TYPE_ERROR || type < TYPE_COUNT) return;
	switch (type_kind(type))
	{
	case TK_PTR: case TK_ARRAY: case TK_SLICE:
		check_export_type(s, types[type].base, use);
		return;
	case TK_FN:
		check_export_fn_type(s, types + type, use);
		return;
	case TK_STRUCT: case TK_CLASS: case TK_ENUM: case TK_ALIAS: case TK_NEWTYPE: {
		ASTNode *decl = named_type_decl(s, type);
		int visible = decl && ((decl->flags & NODE_FLAG_EXPORT)
				|| ((decl->flags & NODE_FLAG_IMPORTED)
					&& (decl->flags & NODE_FLAG_PUB)));

		if (decl && !visible)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + use),
					"export declaration exposes non-export type '%.*s'",
					(int)types[type].len, types[type].name);
		return;
	}
	default: return;
	}
}

static void check_export_fields (Sema *s, uint32_t first)
{
	for (uint32_t m = first; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *n = s->ast->nodes + m;

		if (n->type == NODE_STRUCT_DEC || n->type == NODE_CLASS_DEC)
			check_export_fields(s, n->child);
		else if (n->type == NODE_FIELD_DEC)
			check_export_type(s, n->data_type, m);
	}
}

static void check_export_sig (Sema *s, ASTNode *n)
{
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;
	for (uint32_t item = s->ast->nodes[args].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		check_export_type(s, s->ast->nodes[item].data_type, item);
	check_export_type(s, s->ast->nodes[ret].data_type, ret);
	for (uint32_t item = s->ast->nodes[ret].child; item != NO_NODE;
			item = s->ast->nodes[item].next_bro)
		check_export_type(s, s->ast->nodes[item].data_type, item);
}

static void check_export_method (Sema *s, ASTNode *n)
{
	uint32_t args = n->child;
	uint32_t ret = s->ast->nodes[args].next_bro;

	check_export_name(s, n);
	check_export_sig(s, n);
	if (s->ast->nodes[args].flags & NODE_FLAG_VARARG_TYPED)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"exported class methods cannot be variadic");
	if (s->ast->nodes[ret].child != NO_NODE)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"exported class methods cannot return multiple values");
	else {
		TypeId ret_type = type_unconst_of(s->ast->nodes[ret].data_type);

		if (type_kind(ret_type) == TK_ARRAY || type_dyn_rank(ret_type) > 0)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + ret),
					"an exported class method cannot return arrays or slices");
	}
	for (uint32_t p = s->ast->nodes[args].child; p != NO_NODE;
			p = s->ast->nodes[p].next_bro) {
		TypeId type = type_unconst_of(s->ast->nodes[p].data_type);

		if (type_is_struct_like(type) && types[type].size > 16)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"an exported class method cannot take a struct larger"
					" than 16 bytes by value");
		else if (type_kind(type) == TK_SLICE && type_dyn_rank(type) > 1)
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + p),
					"an exported class method cannot take a slice of rank"
					" greater than 1");
	}
}

static void check_export_methods (Sema *s, uint32_t first)
{
	for (uint32_t m = first; m != NO_NODE; m = s->ast->nodes[m].next_bro) {
		ASTNode *n = s->ast->nodes + m;

		if (n->type != NODE_FN_DEC || (n->flags & NODE_FLAG_PRIV)) continue;
		check_export_method(s, n);
	}
}

static void check_export_decl (Sema *s, ASTNode *n, uint32_t idx)
{
	switch (n->type)
	{
	case NODE_FN_DEC: case NODE_EXTERN_FN_DEC:
		check_export_sig(s, n);
		return;
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_EXTERN_VAR_DEC:
		check_export_type(s, n->data_type, idx);
		return;
	case NODE_STRUCT_DEC: check_export_fields(s, n->child); return;
	case NODE_CLASS_DEC:
		check_export_fields(s, n->child);
		check_export_methods(s, n->child);
		return;
	case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		check_export_type(s, n->data_type, idx);
		return;
	default: return;
	}
}

void check_export_decls (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;

		if (n->flags & NODE_FLAG_EXPORT) {
			check_export_name(s, n);
			check_export_decl(s, n, idx);
		}
		idx = n->next_bro;
	}
}

static const char *type_decl_kind (ASTNode *n)
{
	switch (n->type)
	{
	case NODE_STRUCT_DEC: return (n->flags & NODE_FLAG_UNION) ? "union" : "struct";
	case NODE_CLASS_DEC: return "class";
	case NODE_ENUM_DEC: return "enum";
	case NODE_NEWTYPE_DEC: return "newtype";
	case NODE_TYPE_DEC: return "type";
	default: return NULL;
	}
}

static void warn_unused_fields (Sema *s, Type *t)
{
	if (t->layout != 2) return;

	for (uint16_t i = 0; i < t->field_count; i++) {
		StructField *f = t->fields + i;

		if (f->used || f->name[0] == '_') continue;
		error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + f->decl),
				"unused field '%.*s' of %s '%s'", (int)f->len, f->name,
				t->kind == TK_CLASS ? "class" : t->is_union ? "union" : "struct", t->name);
	}
}

static void warn_unused_variants (Sema *s, Type *t)
{
	for (uint16_t i = 0; i < t->variant_count; i++) {
		EnumVariant *v = t->variants + i;

		if (v->used || v->name[0] == '_') continue;
		error_report(s->err, ERR_WARNING, node_loc(s->ast->nodes + v->decl),
				"unused variant '%.*s' of enum '%s'", (int)v->len, v->name, t->name);
	}
}

static void warn_unused_type (Sema *s, Symbol *sym)
{
	if (sym->decl == NO_NODE) return;
	ASTNode *decl = s->ast->nodes + sym->decl;
	const char *kind = type_decl_kind(decl);
	Type *t = types + sym->type;
	if (!kind || (decl->flags & NODE_FLAG_PUB) || t->exposed) return;
	if (!t->used) {
		if (sym->name[0] == '_') return;
		error_report(s->err, ERR_WARNING, node_loc(decl), "unused %s '%.*s'",
				kind, (int)sym->len, sym->name);
		return;
	}
	if (decl->type == NODE_STRUCT_DEC || decl->type == NODE_CLASS_DEC)
		warn_unused_fields(s, t);
	else if (decl->type == NODE_ENUM_DEC) warn_unused_variants(s, t);
}

static void warn_unused_top (Sema *s)
{
	for (uint32_t i = 0; i < s->table.count; i++) {
		Symbol *sym = s->table.symbols + i;
		ErrorLoc loc = { .line = sym->line, .col = sym->col, .len = sym->len };

		if (sym->depth != 0) continue;
		if (sym->decl != NO_NODE && (s->ast->nodes[sym->decl].flags
				& (NODE_FLAG_PUB | NODE_FLAG_EXPORT)))
			continue;
		if (sym->kind == SYMBOL_TYPE || sym->kind == SYMBOL_ENUM) {
			if (sym->type != TYPE_ERROR) warn_unused_type(s, sym);
			continue;
		}
		if (sym->kind == SYMBOL_FN && sym->decl != NO_NODE
				&& (s->ast->nodes[sym->decl].flags & NODE_FLAG_METHOD)) {
			if ((s->ast->nodes[sym->decl].flags & NODE_FLAG_PRIV)
					&& !sym->read && types[sym->owner].used)
				error_report(s->err, ERR_WARNING, loc,
						"unused private method '%.*s'", (int)sym->len, sym->name);
			continue;
		}
		if (sym->read || is_extern_decl(s, sym) || sym->name[0] == '_') continue;

		if (sym->kind == SYMBOL_FN && !(sym->len == 4
				&& memcmp(sym->name, "main", 4) == 0))
			error_report(s->err, ERR_WARNING, loc,
					"unused function '%.*s'", (int)sym->len, sym->name);
		else if (sym->kind == SYMBOL_VAR)
			error_report(s->err, ERR_WARNING, loc,
					"unused variable '%.*s'", (int)sym->len, sym->name);
	}
}

void sema_run (Sema *s)
{
	reserve_cast_nodes(s);

	decl_types(s);
	sema_resolve_types(s);
	resolve_enum_bases(s);
	sema_finalize_types(s);
	sema_layout_structs(s);
	sema_resolve_types_retry(s);
	types_refresh_const();
	decl_globals(s);
	bind_enums(s);
	types_refresh_const();
	imports_check_collisions(s);

	check_fn_defaults_all(s);
	check_root(s);
	mark_exposed_decls(s);
	check_public_types(s);
	check_export_decls(s);
	warn_unused_top(s);
}
