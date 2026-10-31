#include "sema/common.h"

#include <string.h>

void sema_decl_type (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeKind kind = n->type == NODE_NEWTYPE_DEC ? TK_NEWTYPE : TK_ALIAS;
	TypeId id;

	if (n->len == 4 && memcmp(n->str, "main", 4) == 0) {
		error_report(s->err, ERR_ERROR, node_loc(n), "'main' must be a function");
		return;
	}
	if (symtab_find(&s->table, n->str, n->len) != NO_SYMBOL) {
		sema_declare(s, n->str, n->len, SYMBOL_TYPE, TYPE_ERROR, idx);
		return;
	}

	id = add_type(n->str, n->len, kind, TYPE_ERROR);
	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return;
	}
	sema_declare(s, n->str, n->len, SYMBOL_TYPE, id, idx);
}

static int type_decl_target (Sema *s, ASTNode *n, uint8_t *done, TypeId *out)
{
	if (n->type_node == NO_TYPE_NODE) {
		*out = n->data_type;
		return 1;
	}

	uint32_t ref_idx = (uint32_t)(n - s->ast->nodes) + n->type_node;
	ASTNode *ref = s->ast->nodes + ref_idx;
	TypeId id = find_type(ref->str, ref->len);

	if (id == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(ref),
				"unknown type '%.*s'", (int)ref->len, ref->str);
		*out = TYPE_ERROR;
		return 1;
	}
	if (type_kind(id) == TK_ALIAS && !done[id]) return 0;
	if (type_kind(id) == TK_ALIAS) id = type_canon(id);
	*out = id;
	return 1;
}

static int resolve_one_type (Sema *s, uint32_t idx, uint8_t *done)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym;
	TypeId target;

	if (!(n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC) || n->sym == NO_SYMBOL) return 0;
	sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR || done[sym->type]) return 0;
	if (!type_decl_target(s, n, done, &target)) return 0;

	types[sym->type].base = target;
	done[sym->type] = 1;
	return 1;
}

static void report_unresolved (Sema *s, uint32_t idx, uint8_t *done)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym;

	if (!(n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC) || n->sym == NO_SYMBOL) return;
	sym = s->table.symbols + n->sym;
	if (sym->type == TYPE_ERROR || done[sym->type]) return;

	error_report(s->err, ERR_ERROR, node_loc(n),
			"circular type definition '%.*s'", (int)n->len, n->str);
	types[sym->type].base = TYPE_ERROR;
	done[sym->type] = 1;
}

void sema_resolve_types (Sema *s)
{
	uint8_t *done = arena_alloc(s->arena, type_count);
	uint32_t idx;
	int progress;

	memset(done, 0, type_count);

	do {
		progress = 0;
		idx = s->ast->nodes[0].child;
		while (idx != NO_NODE) {
			progress |= resolve_one_type(s, idx, done);
			idx = s->ast->nodes[idx].next_bro;
		}
	} while (progress);

	idx = s->ast->nodes[0].child;
	while (idx != NO_NODE) {
		report_unresolved(s, idx, done);
		idx = s->ast->nodes[idx].next_bro;
	}
}

static int type_is_broken (TypeId id)
{
	for (int i = 0; i < TYPE_MAX; i++) {
		TypeKind kind = type_kind(id);

		if (kind == TK_ERROR) return 1;
		if (kind != TK_ALIAS && kind != TK_NEWTYPE) return 0;
		if (types[id].base == TYPE_ERROR) return 1;
		id = types[id].base;
	}
	return 0;
}

static void finalize_one_type (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	Symbol *sym;
	TypeId id, repr;

	if (!(n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC) || n->sym == NO_SYMBOL) return;
	sym = s->table.symbols + n->sym;
	id = sym->type;
	if (id == TYPE_ERROR) return;

	repr = type_repr(types[id].base);
	if (repr == TYPE_ERROR) {
		if (!type_is_broken(types[id].base))
			error_report(s->err, ERR_ERROR, node_loc(n),
					"circular type definition '%.*s'",
					(int)n->len, n->str);
		types[id].base = TYPE_ERROR;
		return;
	}
	if (type_kind(id) == TK_NEWTYPE && type_kind(repr) != TK_INT
			&& type_kind(repr) != TK_ENUM) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"newtype underlying type must be an integer type");
		types[id].base = TYPE_ERROR;
		return;
	}

	types[id].size = types[repr].size;
	types[id].sign = types[repr].sign;
}

void sema_finalize_types (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = s->ast->nodes + idx;
		if (n->type == NODE_TYPE_DEC || n->type == NODE_NEWTYPE_DEC) finalize_one_type(s, idx);
		idx = s->ast->nodes[idx].next_bro;
	}
}
