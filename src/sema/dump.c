#include "sema/common.h"

#include <stdio.h>

// debug shit
static const char *kind_name (uint8_t kind)
{
	switch (kind)
	{
	case SYMBOL_FN: return "fn";
	case SYMBOL_VAR: return "var";
	case SYMBOL_PARAM: return "param";
	case SYMBOL_ENUM: return "enum";
	case SYMBOL_TYPE: return "type";
	default: return "unknown";
	}
}

static void dump_variants (TypeId id)
{
	const Type *t = types + id;

	for (uint16_t i = 0; i < t->variant_count; i++) {
		const EnumVariant *v = t->variants + i;

		if (t->sign)
			printf("  variant %.*s = %lld\n", (int)v->len, v->name,
					(long long)(int64_t)v->value);
		else printf("  variant %.*s = %llu\n", (int)v->len, v->name,
				(unsigned long long)v->value);
	}
}

static void dump_fields (TypeId id)
{
	const Type *t = types + id;

	for (uint16_t i = 0; i < t->field_count; i++) {
		const StructField *f = t->fields + i;

		printf("  field %.*s type=%s offset=%u", (int)f->len, f->name,
				types[f->type].name, f->offset);
		if (f->bits) printf(" bits=%u bit_off=%u", f->bits, f->bit_off);
		printf("\n");
	}
}

static void dump_ret_items (Sema *s, Symbol *sym)
{
	if (sym->decl == NO_NODE) return;

	uint32_t ret = s->ast->nodes[s->ast->nodes[sym->decl].child].next_bro;
	uint32_t item = s->ast->nodes[ret].child;

	while (item != NO_NODE) {
		printf(", %s", types[s->ast->nodes[item].data_type].name);
		item = s->ast->nodes[item].next_bro;
	}
}

void dump_symbols (Sema *s)
{
	SymbolTable *t = &s->table;

	for (uint32_t i = 0; i < t->count; i++) {
		Symbol *sym = t->symbols + i;

		if (sym->kind == SYMBOL_ENUM && type_kind(sym->type) == TK_ENUM) {
			TypeId base = types[sym->type].base;

			printf("enum %.*s [%u:%u] depth=%u type=%s base=%s\n",
					(int)sym->len, sym->name, sym->line, sym->col,
					sym->depth, types[sym->type].name, types[base].name);
			dump_variants(sym->type);
			continue;
		}
		if (sym->kind == SYMBOL_TYPE && type_kind(sym->type) == TK_STRUCT) {
			printf("%s %.*s [%u:%u] depth=%u size=%u align=%u%s\n",
					types[sym->type].is_union ? "union" : "struct",
					(int)sym->len, sym->name, sym->line, sym->col, sym->depth,
					types[sym->type].size, types[sym->type].align,
					types[sym->type].is_packed ? " packed" : "");
			dump_fields(sym->type);
			continue;
		}
		if (sym->kind == SYMBOL_TYPE) {
			TypeId id = sym->type;
			const char *kind = type_kind(id) == TK_NEWTYPE
					? "newtype" : "type";

			if (id == TYPE_ERROR || types[id].base == TYPE_ERROR)
				printf("%s %.*s [%u:%u] depth=%u type=error\n",
						kind, (int)sym->len, sym->name,
						sym->line, sym->col, sym->depth);
			else printf("%s %.*s [%u:%u] depth=%u type=%s base=%s\n",
					kind, (int)sym->len, sym->name,
					sym->line, sym->col, sym->depth,
					types[id].name, types[types[id].base].name);
			continue;
		}
		printf("%s%s %.*s [%u:%u] depth=%u type=%s",
				sym->is_static ? "static " : "", kind_name(sym->kind),
				(int)sym->len, sym->name,
				sym->line, sym->col, sym->depth, types[sym->type].name);
		if (sym->kind == SYMBOL_FN) dump_ret_items(s, sym);
		printf("\n");
	}
}
