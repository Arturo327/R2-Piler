#include "r2i/internal.h"

#include <string.h>

static void build_mod_maps (R2i *r)
{
	Sema *s = r->sema;
	AST *ast = r->ir->ast;
	uint32_t nodes = ast->count ? ast->count : 1;

	r->node_mod = arena_alloc(s->arena, sizeof(int32_t) * nodes);
	r->type_mod = arena_alloc(s->arena, sizeof(int32_t) * type_count);
	for (uint32_t i = 0; i < nodes; i++) r->node_mod[i] = -1;
	for (TypeId id = 0; id < type_count; id++) r->type_mod[id] = -1;

	for (uint32_t m = 0; m < s->module_count; m++) {
		ImportModule *mod = s->modules + m;

		for (uint16_t i = 0; i < mod->item_count; i++) {
			uint32_t node = mod->items[i].node;
			ASTNode *n = ast->nodes + node;
			uint8_t kind = n->type;

			if (node < nodes) r->node_mod[node] = (int32_t)m;
			if (kind != NODE_STRUCT_DEC && kind != NODE_ENUM_DEC
					&& kind != NODE_TYPE_DEC && kind != NODE_NEWTYPE_DEC)
				continue;
			uint32_t sym = n->sym;

			if (sym != NO_SYMBOL) {
				TypeId id = r->ir->symtab->symbols[sym].type;

				if (id != TYPE_ERROR && id < type_count)
					r->type_mod[id] = (int32_t)m;
			}
		}
	}
}

static int type_module (R2i *r, TypeId id)
{
	if (id >= type_count) return -1;
	return r->type_mod[id];
}

static void mark_symbol (R2i *r, uint32_t sym_idx)
{
	if (sym_idx == NO_SYMBOL) return;
	uint32_t decl = r->sema->table.symbols[sym_idx].decl;

	if (decl == NO_NODE || decl >= r->ir->ast->count) return;
	int32_t m = r->node_mod[decl];

	if (m >= 0) r->import_used[m] = 1;
}

static void mark_type (R2i *r, TypeId id, uint8_t *seen)
{
	if (id == TYPE_ERROR || id >= type_count || seen[id]) return;
	seen[id] = 1;

	switch (type_kind(id))
	{
	case TK_PTR: case TK_ARRAY: case TK_SLICE:
		mark_type(r, types[id].base, seen);
		return;
	case TK_FN:
		for (uint16_t i = 0; i < types[id].param_count; i++)
			mark_type(r, types[id].params[i], seen);
		for (uint16_t i = 0; i < types[id].ret_count; i++)
			mark_type(r, types[id].rets[i], seen);
		return;
	case TK_STRUCT: case TK_CLASS: case TK_ENUM: case TK_ALIAS: case TK_NEWTYPE: {
		int m = type_module(r, id);

		if (m >= 0) r->import_used[m] = 1;
		if (type_kind(id) == TK_NEWTYPE) mark_type(r, types[id].base, seen);
		return;
	}
	default:
		return;
	}
}

static void mark_expr (R2i *r, uint32_t idx, uint8_t *seen)
{
	if (idx == NO_NODE) return;
	AST *ast = r->ir->ast;
	ASTNode *n = ast->nodes + idx;

	if (n->type == NODE_ID) mark_symbol(r, n->sym);
	if (n->type == NODE_CAST) mark_type(r, n->data_type, seen);
	if (n->type == NODE_INDEX_INIT) mark_expr(r, (uint32_t)n->u64, seen);
	for (uint32_t child = n->child; child != NO_NODE;
			child = ast->nodes[child].next_bro)
		mark_expr(r, child, seen);
}

static void collect_sig (R2i *r, ASTNode *n, uint8_t *seen)
{
	AST *ast = r->ir->ast;
	uint32_t args = n->child;
	uint32_t ret = ast->nodes[args].next_bro;

	for (uint32_t p = ast->nodes[args].child; p != NO_NODE;
			p = ast->nodes[p].next_bro) {
		mark_type(r, ast->nodes[p].data_type, seen);
		if (ast->nodes[p].child != NO_NODE)
			mark_expr(r, ast->nodes[p].child, seen);
	}
	mark_type(r, ast->nodes[ret].data_type, seen);
	for (uint32_t item = ast->nodes[ret].child; item != NO_NODE;
			item = ast->nodes[item].next_bro)
		mark_type(r, ast->nodes[item].data_type, seen);
}

static void collect_fields (R2i *r, ASTNode *n, uint8_t *seen)
{
	AST *ast = r->ir->ast;

	for (uint32_t c = n->child; c != NO_NODE; c = ast->nodes[c].next_bro) {
		ASTNode *m = ast->nodes + c;

		if (m->type == NODE_FIELD_DEC) mark_type(r, m->data_type, seen);
		else if (m->type == NODE_STRUCT_DEC) collect_fields(r, m, seen);
	}
}

static void collect_decl (R2i *r, ASTNode *n, uint8_t *seen)
{
	switch (n->type)
	{
	case NODE_FN_DEC: case NODE_EXTERN_FN_DEC:
		collect_sig(r, n, seen);
		return;
	case NODE_VAR_DEC: case NODE_ARR_DEC: case NODE_EXTERN_VAR_DEC:
		mark_type(r, n->data_type, seen);
		return;
	case NODE_STRUCT_DEC:
		collect_fields(r, n, seen);
		return;
	case NODE_ENUM_DEC: case NODE_TYPE_DEC: case NODE_NEWTYPE_DEC:
		mark_type(r, n->data_type, seen);
		return;
	default:
		return;
	}
}

void r2i_collect_imports (R2i *r)
{
	Sema *s = r->sema;
	AST *ast = r->ir->ast;

	if (!s->module_count) return;
	build_mod_maps(r);
	r->import_used = arena_alloc(s->arena, (size_t)s->module_count + 1u);
	memset(r->import_used, 0, (size_t)s->module_count + 1u);
	uint8_t *seen = arena_alloc(s->arena, (size_t)type_count + 1u);

	memset(seen, 0, (size_t)type_count + 1u);
	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		ASTNode *n = ast->nodes + idx;

		if (!(n->flags & NODE_FLAG_PUB) || (n->flags & NODE_FLAG_IMPORTED))
			continue;
		collect_decl(r, n, seen);
	}
}