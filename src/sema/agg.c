#include "sema/common.h"

#include <string.h>

#define AGG_ANY 0xFFFFFFFEu
#define NO_PLACE 0xFFFFFFFFu
#define AGG_MAX_SLOTS 64

static uint32_t struct_slots (TypeId type)
{
	const Type *t = types + type;

	if (t->is_union || t->field_count == 0 || t->field_count > AGG_MAX_SLOTS)
		return 0;
	for (uint16_t i = 0; i < t->field_count; i++)
		if (t->fields[i].grp) return 0;
	return t->field_count;
}

static uint32_t slot_count (TypeId type)
{
	TypeKind kind = type_kind(type);

	if (kind == TK_STRUCT) return struct_slots(type);
	if (kind == TK_ARRAY)
		return types[type].count <= AGG_MAX_SLOTS ? types[type].count : 0;
	return 0;
}

void agg_declare (Sema *s, uint32_t sym_idx, uint32_t decl_idx)
{
	Symbol *sym = s->table.symbols + sym_idx;
	ASTNode *decl = s->ast->nodes + decl_idx;

	if (sym->kind != SYMBOL_VAR || sym->depth == 0) return;
	if (decl->flags & NODE_FLAG_STATIC) return;

	uint32_t slots = slot_count(sym->type);
	if (!slots) return;

	sym->agg_slots = slots;
	if (decl->child != NO_NODE)
		sym->agg_written = ~(uint64_t)0 >> (64 - slots);
}

void agg_init_all (Sema *s, uint32_t sym_idx)
{
	Symbol *sym = &s->table.symbols[sym_idx];

	if (sym->agg_slots)
		sym->agg_written = ~(uint64_t)0 >> (64 - sym->agg_slots);
}

static void drop_sym_places (Sema *s, uint32_t sym_idx)
{
	uint32_t keep = 0;

	for (uint32_t i = 0; i < s->place_count; i++) {
		if (s->places[i].sym == sym_idx) continue;
		s->places[keep++] = s->places[i];
	}
	s->place_count = keep;
}

void agg_untrack (Sema *s, uint32_t sym_idx)
{
	Symbol *sym = s->table.symbols + sym_idx;
	sym->agg_slots = 0;
	sym->agg_written = 0;
	drop_sym_places(s, sym_idx);
}

void agg_whole_read (Sema *s, uint32_t sym_idx)
{
	Symbol *sym = s->table.symbols + sym_idx;
	if (!sym->agg_slots) return;
	sym->agg_written = ~(uint64_t)0 >> (64 - sym->agg_slots);
	drop_sym_places(s, sym_idx);
}

static uint32_t first_postfix (Sema *s, uint32_t access, uint32_t *root)
{
	uint32_t cur = access;
	uint32_t bottom = access;
	ASTNode *n = s->ast->nodes + cur;

	while (n->type == NODE_DOT || n->type == NODE_INDEX) {
		bottom = cur;
		cur = n->child;
		n = s->ast->nodes + cur;
	}
	*root = cur;
	return bottom;
}

static uint32_t slot_of (Sema *s, uint32_t first)
{
	ASTNode *n = s->ast->nodes + first;

	if (n->type == NODE_DOT) {
		TypeId base = s->ast->nodes[n->child].data_type;
		const StructField *f = find_field(base, n->str, n->len);

		if (!f) return AGG_ANY;
		return (uint32_t)(f - types[base].fields);
	}

	uint64_t bits;
	if (!lit_node_bits(s, s->ast->nodes + s->ast->nodes[n->child].next_bro, &bits))
		return AGG_ANY;
	return bits < AGG_MAX_SLOTS ? (uint32_t)bits : AGG_ANY;
}

static uint32_t resolve_place (Sema *s, uint32_t access, uint32_t *slot, uint32_t *root, uint32_t *first)
{
	uint32_t first_node = first_postfix(s, access, root);
	ASTNode *r = s->ast->nodes + *root;

	*first = first_node;
	if (r->type != NODE_ID || r->sym == NO_SYMBOL) return NO_SYMBOL;

	Symbol *sym = s->table.symbols + r->sym;
	TypeKind kind = type_kind(sym->type);
	if (!sym->agg_slots || sym->addressed) return NO_SYMBOL;
	if (kind != TK_STRUCT && kind != TK_ARRAY) return NO_SYMBOL;

	*slot = slot_of(s, first_node);
	if (*slot != AGG_ANY && *slot >= sym->agg_slots) *slot = AGG_ANY;
	return r->sym;
}

static void warn_missing (Sema *s, Symbol *sym, uint32_t first)
{
	ASTNode *p = s->ast->nodes + first;

	if (p->type == NODE_DOT)
		error_report(s->err, ERR_WARNING, node_loc(p),
				"field '%.*s' of '%.*s' is used without being assigned",
				(int)p->len, p->str, (int)sym->len, sym->name);
	else error_report(s->err, ERR_WARNING, node_loc(p),
			"array element of '%.*s' is used without being assigned",
			(int)sym->len, sym->name);
}

static void drop_slot_places (Sema *s, uint32_t sym_idx, uint32_t slot)
{
	uint32_t keep = 0;

	for (uint32_t i = 0; i < s->place_count; i++) {
		AggPlace *p = s->places + i;

		if (p->sym == sym_idx && p->slot == slot) continue;
		s->places[keep++] = *p;
	}
	s->place_count = keep;
}

void agg_member_read (Sema *s, uint32_t access)
{
	uint32_t root;
	uint32_t first;
	uint32_t slot;
	uint32_t sym_idx = resolve_place(s, access, &slot, &root, &first);
	TypeId type = s->ast->nodes[access].data_type;

	if (sym_idx == NO_SYMBOL || root == s->write_root || s->addr_operand) return;
	if (slot == AGG_ANY) {
		drop_sym_places(s, sym_idx);
		return;
	}
	Symbol *sym = s->table.symbols + sym_idx;
	drop_slot_places(s, sym_idx, slot);
	if (type_is_aggregate(type) || type == TYPE_ERROR || type == TYPE_VOID)
		return;
	if (sym->agg_written & ((uint64_t)1 << slot)) return;

	warn_missing(s, sym, first);
	sym->agg_written |= (uint64_t)1 << slot;
}

static void grow_dead_places (Sema *s)
{
	uint32_t old_cap = s->dead_place_cap;

	s->dead_place_cap <<= 1;
	s->dead_places = arena_realloc(s->arena, s->dead_places,
			sizeof(AggPlace) * old_cap, sizeof(AggPlace) * s->dead_place_cap);
}

static void record_dead_place (Sema *s, const AggPlace *p)
{
	if (s->dead_place_count >= s->dead_place_cap) grow_dead_places(s);
	s->dead_places[s->dead_place_count++] = *p;
}

static uint32_t find_pending (Sema *s, uint32_t sym_idx, uint32_t slot)
{
	for (uint32_t i = 0; i < s->place_count; i++)
		if (s->places[i].sym == sym_idx && s->places[i].slot == slot)
			return i;
	return NO_PLACE;
}

static void grow_places (Sema *s)
{
	uint32_t old_cap = s->place_cap;

	s->place_cap <<= 1;
	s->places = arena_realloc(s->arena, s->places, sizeof(AggPlace) * old_cap,
			sizeof(AggPlace) * s->place_cap);
}

static void write_slot (Sema *s, uint32_t sym_idx, uint32_t slot, uint32_t place, uint32_t node)
{
	Symbol *sym = s->table.symbols + sym_idx;

	if (slot == AGG_ANY) {
		sym->agg_written = ~(uint64_t)0 >> (64 - sym->agg_slots);
		drop_sym_places(s, sym_idx);
		return;
	}
	sym->agg_written |= (uint64_t)1 << slot;

	uint32_t at = find_pending(s, sym_idx, slot);
	if (at != NO_PLACE) record_dead_place(s, s->places + at);
	else {
		if (s->place_count >= s->place_cap) grow_places(s);
		at = s->place_count++;
	}
	s->places[at].sym = sym_idx;
	s->places[at].place = place;
	s->places[at].node = node;
	s->places[at].slot = slot;
}

static void flush_pending (Sema *s, uint32_t sym_idx)
{
	for (uint32_t i = 0; i < s->place_count; i++)
		if (s->places[i].sym == sym_idx)
			record_dead_place(s, s->places + i);
	drop_sym_places(s, sym_idx);
}

void agg_store (Sema *s, uint32_t lvalue, uint32_t node)
{
	ASTNode *n = s->ast->nodes + lvalue;

	if (n->type == NODE_ID) {
		if (n->sym == NO_SYMBOL) return;
		if (!s->table.symbols[n->sym].agg_slots) return;
		s->table.symbols[n->sym].agg_written =
				~(uint64_t)0 >> (64 - s->table.symbols[n->sym].agg_slots);
		flush_pending(s, n->sym);
		return;
	}

	uint32_t root;
	uint32_t first;
	uint32_t slot;
	uint32_t sym_idx = resolve_place(s, lvalue, &slot, &root, &first);

	if (sym_idx == NO_SYMBOL) return;
	write_slot(s, sym_idx, slot, first, node);
}

void agg_untrack_root (Sema *s, uint32_t access)
{
	uint32_t root = access;

	while (s->ast->nodes[root].type == NODE_CAST
			&& s->ast->nodes[root].child != NO_NODE)
		root = s->ast->nodes[root].child;
	if (s->ast->nodes[root].type != NODE_ID)
		first_postfix(s, root, &root);
	if (s->ast->nodes[root].type != NODE_ID) return;
	if (s->ast->nodes[root].sym == NO_SYMBOL) return;
	agg_untrack(s, s->ast->nodes[root].sym);
}

void sema_clear_pending (Sema *s)
{
	symtab_clear_pending(&s->table);
	s->place_count = 0;
}

void sema_load_flags (Sema *s, uint8_t *src, uint32_t mark)
{
	symtab_load_flags(&s->table, src, mark);
	s->place_count = 0;
}

static void warn_dead_place (Sema *s, Symbol *sym, const AggPlace *p)
{
	ASTNode *place = s->ast->nodes + p->place;
	ErrorLoc loc = node_loc(s->ast->nodes + p->node);

	if (place->type == NODE_DOT)
		error_report(s->err, ERR_WARNING, loc,
				"value assigned to field '%.*s' of '%.*s' is not used",
				(int)place->len, place->str, (int)sym->len, sym->name);
	else error_report(s->err, ERR_WARNING, loc,
			"value assigned to an element of '%.*s' is not used",
			(int)sym->len, sym->name);
}

static void drop_sym_dead_places (Sema *s, uint32_t sym_idx)
{
	uint32_t keep = 0;

	for (uint32_t i = 0; i < s->dead_place_count; i++) {
		if (s->dead_places[i].sym == sym_idx) continue;
		s->dead_places[keep++] = s->dead_places[i];
	}
	s->dead_place_count = keep;
}

void sema_pop_places (Sema *s, uint32_t sym_idx, Symbol *sym)
{
	if (sym->read) {
		for (uint32_t i = 0; i < s->dead_place_count; i++)
			if (s->dead_places[i].sym == sym_idx)
				warn_dead_place(s, sym, s->dead_places + i);
		for (uint32_t i = 0; i < s->place_count; i++)
			if (s->places[i].sym == sym_idx)
				warn_dead_place(s, sym, s->places + i);
	}
	drop_sym_dead_places(s, sym_idx);
	drop_sym_places(s, sym_idx);
}
