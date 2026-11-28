#include <stdio.h>
#include <string.h>

#include "imports/internal.h"

#include "mangle/mangle.h"
#include "parser/common.h"
#include "parser/operators.h"
#include "sema/common.h"

static void rewrite_positions (AST *ast, uint32_t start, uint32_t use)
{
	uint16_t line = ast->nodes[use].line;
	uint16_t col = ast->nodes[use].col;

	for (uint32_t i = start; i < ast->count; i++) {
		ast->nodes[i].line = line;
		ast->nodes[i].col = col;
		ast->nodes[i].flags |= NODE_FLAG_IMPORT_BODY;
	}
}

static int decode_symbol (ImportLoader *l, ImportModule *m, const char *sym,
		uint16_t len, const char *kind, const char **name, uint16_t *name_len)
{
	size_t klen = strlen(kind);
	size_t plen = 6u + klen + m->mod_len;
	char *prefix = arena_alloc(&l->c->arena, plen + 1);
	int n = snprintf(prefix, plen + 1, "_r2_%s_%.*s_", kind,
			(int)m->mod_len, m->mod);

	if (n < 0 || (size_t)n != plen) return 0;
	if (len <= plen || memcmp(sym, prefix, plen) != 0) return 0;

	const char *p = sym + plen;
	const char *end = sym + len;
	uint32_t count = 0;

	if (p >= end || *p < '0' || *p > '9') return 0;
	while (p < end && *p >= '0' && *p <= '9') {
		count = count * 10u + (uint32_t)(*p - '0');
		if (count > UINT16_MAX) return 0;
		p++;
	}
	if ((size_t)(end - p) != count) return 0;
	*name = p;
	*name_len = (uint16_t)count;
	return 1;
}

static int is_init_name (ImportModule *m, const char *name, uint16_t len)
{
	static const char prefix[] = "__r2_init_";
	size_t plen = sizeof(prefix) - 1;

	if (len != plen + m->mod_len) return 0;
	if (memcmp(name, prefix, plen) != 0) return 0;
	return memcmp(name + plen, m->mod, m->mod_len) == 0;
}

static int plain_name (ASTNode *n, const char **name, uint16_t *len)
{
	if (n->flags & NODE_FLAG_EXPORT) {
		*name = n->str;
		*len = n->len;
		return 1;
	}
	if (n->type == NODE_EXTERN_FN_DEC || n->type == NODE_EXTERN_VAR_DEC) {
		*name = n->str;
		*len = n->len;
		return 1;
	}
	return 0;
}

static int decl_source_name (ImportLoader *l, ImportModule *m, ASTNode *n,
		const char **name, uint16_t *len)
{
	const char *kind = NULL;

	if (!n->str) return 0;
	if (plain_name(n, name, len)) return 1;
	switch (n->type)
	{
	case NODE_FN_DEC: kind = "pf"; break;
	case NODE_VAR_DEC: case NODE_ARR_DEC: kind = "pv"; break;
	case NODE_STRUCT_DEC: kind = (n->flags & NODE_FLAG_UNION) ? "u" : "s"; break;
	case NODE_CLASS_DEC: kind = "s"; break;
	case NODE_ENUM_DEC: kind = "e"; break;
	case NODE_TYPE_DEC: kind = "a"; break;
	case NODE_NEWTYPE_DEC: kind = "n"; break;
	default: return 0;
	}
	if (decode_symbol(l, m, n->str, n->len, kind, name, len)) return 1;
	fprintf(stderr, "Error: invalid .r2i symbol '%.*s'\n", (int)n->len, n->str);
	l->failed = 1;
	return 0;
}

static const char *method_link (Arena *a, const char *mod,
		const char *cls, uint16_t clen, ASTNode *fn)
{
	if (fn->op != OP_NONE)
		return mangle_class_special(a, mod, cls, clen, op_canon(fn->op));
	if (fn->flags & NODE_FLAG_INIT)
		return mangle_class_special(a, mod, cls, clen, "init");
	if (fn->flags & NODE_FLAG_DESTROY)
		return mangle_class_special(a, mod, cls, clen, "destroy");

	MangleName name = {
		.kind = "m", .mod = mod, .a = cls, .alen = clen,
		.b = fn->str, .blen = fn->len
	};

	return mangle_symbol(a, &name);
}

static const char *module_text (Arena *a, ImportModule *m)
{
	char *mod = arena_alloc(a, (size_t)m->mod_len + 1);

	memcpy(mod, m->mod, m->mod_len);
	mod[m->mod_len] = '\0';
	return mod;
}

static void record_class_links (ImportLoader *l, ImportModule *m,
		uint32_t node, const char *name, uint16_t len)
{
	AST *ast = &l->c->parser.ast;
	ASTNode *cls = &ast->nodes[node];
	const char *mod = module_text(&l->c->arena, m);

	for (uint32_t c = cls->child; c != NO_NODE; c = ast->nodes[c].next_bro) {
		ASTNode *fn = &ast->nodes[c];

		if (fn->type != NODE_FN_DEC) continue;
		ast->nodes[c].flags |= NODE_FLAG_IMPORTED;
		sema_add_import_link(&l->c->sema, c,
				method_link(&l->c->arena, mod, name, len, fn));
	}
}

static void add_item (ImportLoader *l, ImportModule *m, ImportedDecl item)
{
	if (m->item_count >= UINT16_MAX) {
		fprintf(stderr, "Error: too many declarations in an imported module\n");
		l->failed = 1;
		return;
	}
	if (m->item_count >= m->item_cap) {
		uint16_t old = m->item_cap;
		uint32_t next = old ? (uint32_t)old << 1 : 8u;

		if (next > UINT16_MAX) next = UINT16_MAX;
		m->item_cap = (uint16_t)next;
		m->items = arena_realloc(&l->c->arena, m->items,
				sizeof(ImportedDecl) * old, sizeof(ImportedDecl) * m->item_cap);
	}
	m->items[m->item_count++] = item;
}

static void add_decl (ImportLoader *l, ImportModule *m, uint32_t node)
{
	ASTNode *n = &l->c->parser.ast.nodes[node];
	const char *name;
	uint16_t len;

	if (n->type == NODE_FN_DEC && n->str && is_init_name(m, n->str, n->len)) {
		m->init_node = node;
		return;
	}
	if (!decl_source_name(l, m, n, &name, &len)) return;
	ImportedDecl item = { name, len, node };

	add_item(l, m, item);
	if (n->type == NODE_CLASS_DEC) record_class_links(l, m, node, name, len);
}

static int collect_body_decls (ImportLoader *l, uint32_t module_idx,
		uint32_t first, const char *path)
{
	AST *ast = &l->c->parser.ast;
	ImportModule *m = l->c->sema.modules + module_idx;

	for (uint32_t node = first; node != NO_NODE; node = ast->nodes[node].next_bro) {
		ast->nodes[node].flags |= NODE_FLAG_IMPORTED;
		add_decl(l, m, node);
	}
	if (m->has_init && m->init_node == NO_NODE) {
		fprintf(stderr, "Error: invalid .r2i file '%s': missing __r2_init prototype\n",
				path);
		l->failed = 1;
		return 1;
	}
	return 0;
}

int import_parse_body (ImportLoader *l, uint32_t module_idx, char *src,
		const char *path, uint32_t use)
{
	Compiler *c = l->c;
	AST *ast = &c->parser.ast;
	ErrorReporter er;
	Lexer lex;
	Parser p;

	error_init(&er, path, src, &c->arena);
	init_lexer(&lex, src, &c->arena, &er);
	lex.r2i_mode = 1;
	uint32_t start = ast->count;
	uint32_t last = root_last_child(ast);

	init_parser_shared(&p, &lex, c->parser.arena, &er, ast);
	parse(&p);
	c->parser.ast = p.ast;
	ast = &c->parser.ast;
	if (er.err_count) {
		l->failed = 1;
		return 1;
	}

	rewrite_positions(ast, start, use);
	uint32_t first = last == NO_NODE ? ast->nodes[0].child
			: ast->nodes[last].next_bro;

	return collect_body_decls(l, module_idx, first, path);
}
