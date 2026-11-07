#include "sema/common.h"

#include <string.h>

#define ALIGN_UP(v, a) (((v) + (a) - 1) & ~((uint64_t)(a) - 1))

static int layout_struct (Sema *s, TypeId id);

void sema_decl_struct (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;

	if (!n->str) {
		error_report(s->err, ERR_ERROR, node_loc(n), "a top-level struct needs a name");
		return;
	}
	if (n->len == 4 && memcmp(n->str, "main", 4) == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'main' must be a function");
		return;
	}
	if (symtab_find(&s->table, n->str, n->len) != NO_SYMBOL) {
		sema_declare(s, n->str, n->len, SYMBOL_TYPE, TYPE_ERROR, idx);
		return;
	}

	TypeId id = add_type(n->str, n->len, TK_STRUCT, TYPE_VOID);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return;
	}
	types[id].base = id;
	types[id].decl = idx;
	sema_declare(s, n->str, n->len, SYMBOL_TYPE, id, idx);
}

static TypeId field_struct_dep (Sema *s, uint32_t idx)
{
	ASTNode *f = s->ast->nodes + idx;
	ASTNode *ref;
	TypeId id;

	if (f->type_node == NO_TYPE_NODE) return TYPE_ERROR;
	ref = f + f->type_node;
	while (ref->type == NODE_TYPE_ARRAY && ref->type_node != NO_TYPE_NODE)
		ref = ref + ref->type_node;
	if (ref->type != NODE_TYPE_REF) return TYPE_ERROR;

	id = type_canon(find_type(ref->str, ref->len));
	return type_kind(id) == TK_STRUCT ? id : TYPE_ERROR;
}

static TypeId field_type (Sema *s, uint32_t idx)
{
	TypeId dep = field_struct_dep(s, idx);
	ASTNode *f = s->ast->nodes + idx;

	if (dep != TYPE_ERROR && types[dep].layout == 1) {
		error_report(s->err, ERR_ERROR, node_loc(f),
				"field '%.*s' has incomplete type '%s';"
				" a struct cannot contain itself, use a pointer",
				(int)f->len, f->str, types[dep].name);
		f->type_node = NO_TYPE_NODE;
		f->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (dep != TYPE_ERROR) layout_struct(s, dep);

	TypeId t = resolve_type_node(s, s->ast->nodes + idx);
	f = s->ast->nodes + idx;
	if (t == TYPE_ERROR || !types[t].size) return TYPE_ERROR;
	if (type_dyn_rank(t) > 0) {
		error_report(s->err, ERR_ERROR, node_loc(f),
				"slice types are only allowed as function parameters");
		f->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return t;
}

static void add_field (Sema *s, StructField *out, uint32_t *count,
		uint32_t idx, TypeId type, uint64_t off)
{
	ASTNode *m = s->ast->nodes + idx;
	StructField *f = out + *count;

	for (uint32_t i = 0; i < *count; i++) {
		if (out[i].len != m->len || memcmp(out[i].name, m->str, m->len))
			continue;
		error_report(s->err, ERR_ERROR, node_loc(m),
				"duplicate field '%.*s'", (int)m->len, m->str);
		return;
	}
	f->name = m->str;
	f->len = m->len;
	f->type = type;
	f->offset = (uint32_t)off;
	(*count)++;
}

static void place (Sema *s, StructField *out, uint32_t *count,
		uint32_t node, uint64_t base, uint64_t *size, uint32_t *align, int record)
{
	uint32_t c = s->ast->nodes[node].child;
	uint64_t off = 0;
	uint32_t al = 1;

	for (; c != NO_NODE; c = s->ast->nodes[c].next_bro) {
		ASTNode *m = s->ast->nodes + c;
		uint64_t fs = 0;
		uint32_t fa = 1;

		if (m->type == NODE_FIELD_DEC) {
			TypeId ft = field_type(s, c);

			if (ft == TYPE_ERROR) continue;
			fa = type_align(ft);
			fs = types[ft].size;
			off = ALIGN_UP(off, fa);
			if (record) add_field(s, out, count, c, ft, base + off);
		} else if (m->str) {
			if (record) error_report(s->err, ERR_ERROR, node_loc(m),
					"a named struct cannot be declared inside another struct");
			continue;
		} else {
			place(s, out, count, c, 0, &fs, &fa, 0);
			off = ALIGN_UP(off, fa);
			if (record) place(s, out, count, c, base + off, &fs, &fa, 1);
		}
		off += fs;
		if (fa > al) al = fa;
	}
	*size = ALIGN_UP(off, al);
	*align = al;
}

static uint32_t count_fields (Sema *s, uint32_t node)
{
	uint32_t c = s->ast->nodes[node].child;
	uint32_t n = 0;

	for (; c != NO_NODE; c = s->ast->nodes[c].next_bro) {
		if (s->ast->nodes[c].type == NODE_FIELD_DEC) n++;
		else n += count_fields(s, c);
	}
	return n;
}

static int layout_struct (Sema *s, TypeId id)
{
	Type *t = types + id;
	int errs = s->err->err_count;
	uint64_t size;
	uint32_t align;
	uint32_t count = 0;
	StructField *out = arena_alloc(s->arena, sizeof(StructField) * (count_fields(s, t->decl) + 1u));

	if (t->layout == 2) return 1;
	if (t->layout != 0) return 0;

	t->layout = 1;
	place(s, out, &count, t->decl, 0, &size, &align, 1);

	if (!count && s->err->err_count == errs)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + t->decl),
				"struct '%s' must have at least one field", t->name);
	if (size > UINT32_MAX || count > UINT16_MAX)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + t->decl),
				"struct '%s' is too large", t->name);
	if (s->err->err_count != errs) {
		t->layout = 3;
		return 0;
	}
	bind_struct_type(id, out, (uint16_t)count, (uint32_t)size, align);
	t->layout = 2;
	return 1;
}

void sema_layout_structs (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	for (; idx != NO_NODE; idx = s->ast->nodes[idx].next_bro) {
		ASTNode *n = s->ast->nodes + idx;
		TypeId id;

		if (n->type != NODE_STRUCT_DEC || n->sym == NO_SYMBOL) continue;
		id = s->table.symbols[n->sym].type;
		if (type_kind(id) == TK_STRUCT) layout_struct(s, id);
	}
}

static void check_field_value (Sema *s, uint32_t item, const StructField *f)
{
	ASTNode *fi = s->ast->nodes + item;
	uint32_t val = fi->child;
	uint8_t vt = s->ast->nodes[val].type;
	TypeKind kind = type_kind(f->type);
	TypeId type;
	int ok;

	if (kind == TK_STRUCT && vt == NODE_STRUCT_INIT) {
		check_struct_init(s, val, f->type);
	} else if (kind == TK_ARRAY && (vt == NODE_ARRAY_INIT || vt == NODE_LIT_STR)) {
		check_array_init_at(s, f->type, val);
	} else if (vt == NODE_ARRAY_INIT || vt == NODE_STRUCT_INIT || kind == TK_ARRAY) {
		error_report(s->err, ERR_ERROR, node_loc(fi),
				"field '%.*s' of type %s cannot be initialized this way",
				(int)fi->len, fi->str, types[f->type].name);
	} else {
		type = check_expr(s, val);
		if (type == TYPE_ERROR || type == f->type) return;
		fi->child = force_cast(s, val, type, f->type, &ok);
		if (ok) return;
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + item),
				"cannot initialize field '%.*s' of type %s with a value of type %s;"
				" use an explicit cast with 'as'",
				(int)f->len, f->name, types[f->type].name, type_display_name(type));
	}
}

static void check_field_init (Sema *s, uint32_t item, TypeId st, uint8_t *seen)
{
	ASTNode *fi = s->ast->nodes + item;
	const StructField *f = find_field(st, fi->str, fi->len);

	if (!f) {
		error_report(s->err, ERR_ERROR, node_loc(fi), "struct '%s' has no field '%.*s'",
				types[st].name, (int)fi->len, fi->str);
		return;
	}
	if (seen[f - types[st].fields])
		error_report(s->err, ERR_ERROR, node_loc(fi),
				"duplicate field '%.*s' in the initializer", (int)fi->len, fi->str);
	seen[f - types[st].fields] = 1;
	check_field_value(s, item, f);
}

void check_struct_init (Sema *s, uint32_t init_idx, TypeId st)
{
	uint32_t count = types[st].field_count;
	uint8_t *seen = arena_alloc(s->arena, count + 1u);
	uint32_t item = s->ast->nodes[init_idx].child;

	memset(seen, 0, count + 1u);
	s->ast->nodes[init_idx].data_type = st;
	while (item != NO_NODE) {
		check_field_init(s, item, st, seen);
		item = s->ast->nodes[item].next_bro;
	}
	for (uint32_t k = 0; k < count; k++) {
		if (seen[k]) continue;
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + init_idx),
				"missing field '%.*s' in the initializer of struct '%s'",
				(int)types[st].fields[k].len, types[st].fields[k].name, types[st].name);
	}
}
