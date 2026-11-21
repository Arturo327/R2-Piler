#include <string.h>

#include "imports/internal.h"
#include "sema/common.h"

static int alias_find (Sema *s, const char *name, uint16_t len, uint32_t *module)
{
	for (uint32_t i = 0; i < s->alias_count; i++) {
		ImportAlias *a = s->aliases + i;

		if (a->len == len && memcmp(a->name, name, len) == 0) {
			*module = a->module;
			return 1;
		}
	}
	return 0;
}

static int binding_find (Sema *s, const char *name, uint16_t len)
{
	for (uint32_t i = 0; i < s->binding_count; i++) {
		ImportBinding *b = s->bindings + i;

		if (b->len == len && memcmp(b->name, name, len) == 0) return (int)i;
	}
	return -1;
}

ImportedDecl *imports_lookup (Sema *s, const char *name, uint16_t len)
{
	uint16_t sep = 0;

	for (uint16_t i = 0; i + 1 < len; i++)
		if (name[i] == ':' && name[i + 1] == ':') sep = i;
	if (sep && sep + 1 < len) {
		uint32_t mod;
		uint16_t mlen = (uint16_t)(len - sep - 2);

		if (!sep || !mlen) return NULL;
		if (!alias_find(s, name, sep, &mod)) return NULL;
		return import_find_item(s->modules + mod, name + sep + 2, mlen);
	}
	int idx = binding_find(s, name, len);

	return idx < 0 ? NULL : s->bindings[idx].item;
}

uint32_t sema_lookup (Sema *s, const char *name, uint16_t len)
{
	uint32_t sym = symtab_find(&s->table, (char *)name, len);

	if (sym != NO_SYMBOL) return sym;
	ImportedDecl *decl = imports_lookup(s, name, len);
	if (!decl) return NO_SYMBOL;
	return s->ast->nodes[decl->node].sym;
}

TypeId sema_lookup_type (Sema *s, const char *name, uint16_t len)
{
	TypeId id = find_type(name, len);

	if (id != TYPE_ERROR) return id;
	ImportedDecl *decl = imports_lookup(s, name, len);

	if (!decl) return TYPE_ERROR;
	uint32_t sym = s->ast->nodes[decl->node].sym;

	if (sym == NO_SYMBOL) return TYPE_ERROR;
	Symbol *sy = &s->table.symbols[sym];

	if (sy->kind != SYMBOL_TYPE && sy->kind != SYMBOL_ENUM) return TYPE_ERROR;
	types[sy->type].used = 1;
	return sy->type;
}

void imports_check_collisions (Sema *s)
{
	for (uint32_t i = 0; i < s->binding_count; i++) {
		ImportBinding *b = s->bindings + i;
		uint32_t sym = symtab_find(&s->table, (char *)b->name, b->len);
		ImportModule *m = s->modules + b->module;

		if (sym == NO_SYMBOL) continue;
		Symbol *found = &s->table.symbols[sym];

		if (found->decl != NO_NODE
				&& (s->ast->nodes[found->decl].flags & NODE_FLAG_IMPORTED))
			continue;
		ErrorLoc loc = { found->line, found->col, found->len };

		if (found->decl != NO_NODE)
			loc = node_loc(&s->ast->nodes[found->decl]);
		error_report(s->err, ERR_ERROR, loc,
				"'%.*s' conflicts with the name imported from module '%.*s'",
				(int)b->len, b->name, (int)m->name_len, m->name);
	}
}
