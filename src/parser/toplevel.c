#include "parser/common.h"

static void module_path_error (Parser *p, const char *msg)
{
	if (!p->panic_mode)
		error_report(p->err, ERR_ERROR, token_loc(p->curr), "%s", msg);
}

static int check_module_path (Parser *p, Token name)
{
	for (uint32_t i = 0; i < name.len; i++) {
		if (name.str[i] == ':') {
			module_path_error(p, "module path cannot contain ':'; use '/' as the separator");
			return 0;
		}
		if (name.str[i] != '/') continue;
		if (!i || name.str[i - 1] == '/' || i + 1 == name.len) {
			module_path_error(p, "module path has an empty segment");
			return 0;
		}
	}
	return 1;
}

static char *parse_module_name (Parser *p, const char *what, uint16_t *len)
{
	if (p->curr.type != TOK_LIT_STR) {
		module_path_error(p, what);
		p->panic_mode = 1;
		*len = 0;
		return NULL;
	}
	if (!p->curr.len) {
		module_path_error(p, "module name cannot be empty");
		p->panic_mode = 1;
		*len = 0;
		return NULL;
	}
	if (!check_module_path(p, p->curr)) {
		p->panic_mode = 1;
		*len = 0;
		return NULL;
	}
	*len = p->curr.len;
	char *name = p->curr.str;

	advance(p);
	return name;
}

uint32_t parse_import (Parser *p, int root)
{
	Token kw = p->curr;

	advance(p);
	uint32_t node = new_node(p, NODE_IMPORT_DEC, kw.line, kw.col);

	p->ast.nodes[node].str = parse_module_name(p,
			"expected a module name string after 'import'", &p->ast.nodes[node].len);
	if (p->curr.type == TOK_AS) {
		advance(p);
		uint32_t alias = new_node(p, NODE_ID, p->curr.line, p->curr.col);

		if (consume(p, TOK_ID, "expected an alias after 'as'")) {
			p->ast.nodes[alias].str = p->prev.str;
			p->ast.nodes[alias].len = p->prev.len;
		}
		p->ast.nodes[node].child = alias;
	}
	consume(p, TOK_SEMCOL, "expected ';' after the import");
	if (!root) error_report(p->err, ERR_ERROR, token_loc(kw),
			"import declarations are only allowed at the top level");
	return node;
}

static void parse_from_item (Parser *p, uint32_t from, uint32_t *last)
{
	uint32_t item = new_node(p, NODE_ID, p->curr.line, p->curr.col);

	if (consume(p, TOK_ID, "expected a name to import")) {
		p->ast.nodes[item].str = p->prev.str;
		p->ast.nodes[item].len = p->prev.len;
	}
	if (p->curr.type == TOK_AS) {
		advance(p);
		uint32_t alias = new_node(p, NODE_ID, p->curr.line, p->curr.col);

		if (consume(p, TOK_ID, "expected an alias after 'as'")) {
			p->ast.nodes[alias].str = p->prev.str;
			p->ast.nodes[alias].len = p->prev.len;
		}
		p->ast.nodes[item].child = alias;
	}
	append_child(p, from, last, item);
}

uint32_t parse_from (Parser *p, int root)
{
	Token kw = p->curr;

	advance(p);
	uint32_t node = new_node(p, NODE_FROM_DEC, kw.line, kw.col);

	p->ast.nodes[node].str = parse_module_name(p,
			"expected a module name string after 'from'", &p->ast.nodes[node].len);
	consume(p, TOK_IMPORT, "expected 'import' after the module name");
	uint32_t last = NO_NODE;

	while (1) {
		parse_from_item(p, node, &last);
		if (p->curr.type != TOK_COMMA || p->panic_mode) break;
		advance(p);
	}
	consume(p, TOK_SEMCOL, "expected ';' after the import list");
	if (!root) error_report(p->err, ERR_ERROR, token_loc(kw),
			"import declarations are only allowed at the top level");
	return node;
}

static int pub_target (TokenType type)
{
	switch (type)
	{
	case TOK_FN: case TOK_VAR: case TOK_ARR: case TOK_ENUM:
	case TOK_STRUCT: case TOK_UNION: case TOK_PACKED:
	case TOK_TYPE: case TOK_NEWTYPE: case TOK_EXTERN: case TOK_CLASS:
		return 1;
	default:
		return 0;
	}
}

static uint32_t mark_visibility (Parser *p, uint32_t node, uint16_t flags)
{
	for (uint32_t k = node; k != NO_NODE; k = p->ast.nodes[k].next_bro)
		p->ast.nodes[k].flags |= flags;
	return node;
}

void mark_decl_flags (Parser *p, uint32_t node, uint16_t flags)
{
	if (p->ast.nodes[node].type == NODE_MULTI_ASSIGN) {
		for (uint32_t k = p->ast.nodes[node].child; k != NO_NODE;
				k = p->ast.nodes[k].next_bro)
			p->ast.nodes[k].flags |= flags;
		return;
	}
	mark_visibility(p, node, flags);
}

uint32_t parse_pub (Parser *p, int root)
{
	Token pub = p->curr;
	const char *msg = "expected a declaration after 'pub'";

	advance(p);
	int ok = pub_target(p->curr.type);
	if (p->curr.type == TOK_EXPORT) {
		msg = "'pub' cannot be combined with 'export'";
		ok = 0;
	} else if (!ok && p->curr.type == TOK_STATIC) {
		msg = "'static' cannot be 'pub'";
	}
	if (!ok) {
		if (!p->panic_mode) error_report(p->err, ERR_ERROR, token_loc(pub), "%s", msg);
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, pub.line, pub.col);
	}
	return mark_visibility(p, parse_statement_inner(p, root), NODE_FLAG_PUB);
}

uint32_t parse_export (Parser *p, int root)
{
	Token exp = p->curr;
	const char *msg = "expected a declaration after 'export'";

	advance(p);
	int ok = pub_target(p->curr.type) && p->curr.type != TOK_EXTERN;
	if (p->curr.type == TOK_PUB) {
		msg = "'export' cannot be combined with 'pub'";
		ok = 0;
	}
	if (!ok) {
		if (!p->panic_mode) error_report(p->err, ERR_ERROR, token_loc(exp), "%s", msg);
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, exp.line, exp.col);
	}
	return mark_visibility(p, parse_statement_inner(p, root),
			NODE_FLAG_PUB | NODE_FLAG_EXPORT);
}

uint32_t parse_module (Parser *p)
{
	Token kw = p->curr;

	advance(p);
	if (p->curr.type != TOK_LIT_STR) {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"expected a module name string after 'module'");
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, kw.line, kw.col);
	}
	Token name = p->curr;

	advance(p);
	consume(p, TOK_SEMCOL, "expected ';' after the module name");
	uint32_t node = new_node(p, NODE_MODULE_DEC, kw.line, kw.col);

	p->ast.nodes[node].str = name.str;
	p->ast.nodes[node].len = name.len;
	return node;
}
