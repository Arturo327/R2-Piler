#include "parser/common.h"

static char *parse_module_path (Parser *p, const char *what, uint16_t *len)
{
	Token first = p->curr;

	if (!consume(p, TOK_ID, what)) {
		*len = first.len;
		return first.str;
	}
	int ok;
	char *name = qualify_name(p, first, len, &ok);

	(void)ok;
	return name;
}

uint32_t parse_import (Parser *p, int root)
{
	Token kw = p->curr;

	advance(p);
	uint32_t node = new_node(p, NODE_IMPORT_DEC, kw.line, kw.col);

	p->ast.nodes[node].str = parse_module_path(p,
			"expected a module name after 'import'", &p->ast.nodes[node].len);
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

	p->ast.nodes[node].str = parse_module_path(p,
			"expected a module name after 'from'", &p->ast.nodes[node].len);
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
	case TOK_TYPE: case TOK_NEWTYPE: case TOK_EXTERN:
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
