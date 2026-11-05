#include "parser/common.h"

static uint32_t parse_array_init (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_ARRAY_INIT, line, col);
	uint32_t last = NO_NODE;

	consume(p, TOK_LKEY, "expected '{' for array initializer");

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		uint32_t item = p->curr.type == TOK_LKEY
				? parse_array_init(p) : parse_expr(p, 0);

		append_child(p, node, &last, item);
		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RKEY, "expected '}' at the end of the array initializer");
	return node;
}

static uint32_t parse_decl_inner (Parser *p, int is_arr)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, is_arr ? NODE_ARR_DEC : NODE_VAR_DEC, line, col);

	if (consume(p, TOK_ID, "expected variable name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	consume(p, TOK_COL, "expected ':', followed by the type");
	uint16_t type_node;
	TypeId type = parse_type(p, 0, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;

	if (p->curr.type == TOK_ASSIGN) {
		advance(p);
		uint32_t value = is_arr && p->curr.type == TOK_LKEY
				? parse_array_init(p) : parse_expr(p, 0);
		p->ast.nodes[node].child = value;
	}

	return node;
}

static uint32_t parse_decl (Parser *p, int is_arr)
{
	consume(p, is_arr ? TOK_ARR : TOK_VAR,
			is_arr ? "expected 'arr'" : "expected 'var'");

	uint32_t first = parse_decl_inner(p, is_arr);
	uint32_t last = first;

	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		uint32_t next = parse_decl_inner(p, is_arr);
		p->ast.nodes[last].next_bro = next;
		last = next;
	}

	consume(p, TOK_SEMCOL, "expected ';' at the end of the declaration");
	return first;
}

uint32_t parse_block (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_LKEY, "expected '{' for block opening");

	uint32_t block = new_node(p, NODE_BLOCK, line, col);
	uint32_t last = NO_NODE;

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		Token start = p->curr;
		uint32_t stmt = parse_statement(p);
		append_child(p, block, &last, stmt);
		if (p->panic_mode) syncro(p, start);
	}

	consume(p, TOK_RKEY, "expected '}' for block closing");
	return block;
}

static uint32_t parse_return (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_RET, "expected 'return'");

	uint32_t node = new_node(p, NODE_RET, line, col);
	if (p->curr.type != TOK_SEMCOL) {
		uint32_t value = parse_expr(p, 0);
		p->ast.nodes[node].child = value;
	}

	consume(p, TOK_SEMCOL, "expected ';' after return statement");
	return node;
}

static inline uint32_t parse_cond (Parser *p, const char *msg)
{
	consume(p, TOK_LPAREN, msg);
	uint32_t cond = parse_expr(p, 0);
	consume(p, TOK_RPAREN, "expected ')'");
	return cond;
}

static uint32_t parse_body (Parser *p)
{
	uint32_t body = parse_statement(p);

	if (p->ast.nodes[body].next_bro == NO_NODE) return body;

	uint16_t line = p->ast.nodes[body].line;
	uint16_t col = p->ast.nodes[body].col;
	uint32_t block = new_node(p, NODE_BLOCK, line, col);

	p->ast.nodes[block].child = body;
	return block;
}

static void parse_elif_chain (Parser *p, uint32_t if_node, uint32_t *last)
{
	while (p->curr.type == TOK_ELIF) {
		uint16_t eline = p->curr.line;
		uint16_t ecol = p->curr.col;
		consume(p, TOK_ELIF, "expected 'elif'");

		uint32_t elif_node = new_node(p, NODE_ELIF, eline, ecol);
		uint32_t elif_cond = parse_cond(p, "expected condition after elif statement");
		uint32_t elif_body = parse_body(p);

		uint32_t elif_last = NO_NODE;
		append_child(p, if_node, last, elif_node);
		append_child(p, elif_node, &elif_last, elif_cond);
		append_child(p, elif_node, &elif_last, elif_body);
	}

	if (p->curr.type != TOK_ELSE) return;

	uint16_t eline = p->curr.line;
	uint16_t ecol = p->curr.col;
	consume(p, TOK_ELSE, "expected 'else'");

	uint32_t else_node = new_node(p, NODE_ELSE, eline, ecol);
	uint32_t else_body = parse_body(p);

	uint32_t else_last = NO_NODE;
	append_child(p, if_node, last, else_node);
	append_child(p, else_node, &else_last, else_body);
}

static uint32_t parse_if (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_IF, "expected 'if'");

	uint32_t node = new_node(p, NODE_IF, line, col);
	uint32_t cond = parse_cond(p, "expected condition after if statement");
	uint32_t body = parse_body(p);

	uint32_t last = NO_NODE;
	append_child(p, node, &last, cond);
	append_child(p, node, &last, body);
	parse_elif_chain(p, node, &last);

	return node;
}

static uint32_t parse_while (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_WHILE, "expected 'while'");

	uint32_t node = new_node(p, NODE_WHILE, line, col);
	uint32_t cond = parse_cond(p, "expected condition after while statement");
	uint32_t body = parse_body(p);

	uint32_t last = NO_NODE;
	append_child(p, node, &last, cond);
	append_child(p, node, &last, body);

	return node;
}

static uint32_t parse_for_init (Parser *p)
{
	if (p->curr.type == TOK_SEMCOL && !p->panic_mode) {
		uint32_t e = new_node(p, NODE_EMPTY, p->curr.line, p->curr.col);
		advance(p);
		return e;
	}
	if (p->curr.type == TOK_VAR && !p->panic_mode)
		return parse_decl(p, 0);
	if (p->curr.type == TOK_ARR && !p->panic_mode)
		return parse_decl(p, 1);
	uint32_t e = parse_expr(p, 0);
	consume(p, TOK_SEMCOL, "expected ';' between for expressions");
	return e;
}

static uint32_t parse_for_cond (Parser *p)
{
	if (p->curr.type == TOK_SEMCOL && !p->panic_mode) {
		uint32_t e = new_node(p, NODE_EMPTY, p->curr.line, p->curr.col);
		advance(p);
		return e;
	}
	uint32_t c = parse_expr(p, 0);
	consume(p, TOK_SEMCOL, "expected ';' between for expressions");
	return c;
}

static uint32_t parse_for_updt (Parser *p)
{
	if (p->curr.type == TOK_RPAREN && !p->panic_mode) {
		uint32_t e = new_node(p, NODE_EMPTY, p->curr.line, p->curr.col);
		advance(p);
		return e;
	}

	uint32_t first = maybe_inc_dec(p, parse_expr(p, 0));
	uint32_t last = first;

	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		uint32_t next = maybe_inc_dec(p, parse_expr(p, 0));

		p->ast.nodes[last].next_bro = next;
		last = next;
	}
	consume(p, TOK_RPAREN, "expected ')'");
	if (first == last) return first;

	uint16_t line = p->ast.nodes[first].line;
	uint16_t col = p->ast.nodes[first].col;
	uint32_t block = new_node(p, NODE_BLOCK, line, col);

	p->ast.nodes[block].child = first;
	return block;
}

static uint32_t wrap_for_decls (Parser *p, uint32_t decls, uint32_t loop)
{
	uint32_t tail = decls;
	uint16_t line = p->ast.nodes[loop].line;
	uint16_t col = p->ast.nodes[loop].col;

	while (p->ast.nodes[tail].next_bro != NO_NODE)
		tail = p->ast.nodes[tail].next_bro;

	p->ast.nodes[tail].next_bro = loop;

	uint32_t block = new_node(p, NODE_BLOCK, line, col);

	p->ast.nodes[block].child = decls;
	return block;
}

static uint32_t parse_for (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_FOR, "expected 'for'");
	uint32_t node = new_node(p, NODE_FOR, line, col);

	consume(p, TOK_LPAREN, "expected '('");
	uint32_t beg = parse_for_init(p);
	uint32_t decls = NO_NODE;

	if (p->ast.nodes[beg].next_bro != NO_NODE) {
		decls = beg;
		beg = new_node(p, NODE_EMPTY, line, col);
	}
	uint32_t cond = parse_for_cond(p);
	uint32_t updt = parse_for_updt(p);
	uint32_t body = parse_body(p);

	uint32_t last = NO_NODE;
	append_child(p, node, &last, beg);
	append_child(p, node, &last, cond);
	append_child(p, node, &last, updt);
	append_child(p, node, &last, body);

	if (decls == NO_NODE) return node;
	return wrap_for_decls(p, decls, node);
}

static uint32_t error_prefix_inc (Parser *p)
{
	Token op = p->curr;

	error_report(p->err, ERR_ERROR, token_loc(op),
			"'%s' cannot be used as a prefix; use the postfix form as a statement",
			op.type == TOK_INC ? "++" : "--");

	while (p->curr.type != TOK_SEMCOL && p->curr.type != TOK_EOF)
		advance(p);
	if (p->curr.type == TOK_SEMCOL) advance(p);
	return new_node(p, NODE_ERROR, op.line, op.col);
}

static uint32_t parse_statement_inner (Parser *p)
{
	switch (p->curr.type)
	{
	case TOK_VAR: return parse_decl(p, 0);
	case TOK_ARR: return parse_decl(p, 1);
	case TOK_LKEY: return parse_block(p);
	case TOK_FN: return parse_fn_decl(p);
	case TOK_EXTERN: return parse_extern_decl(p);
	case TOK_ENUM: return parse_enum(p);
	case TOK_TYPE: return parse_type_dec(p, 0);
	case TOK_NEWTYPE: return parse_type_dec(p, 1);
	case TOK_RET: return parse_return(p);
	case TOK_IF: return parse_if(p);
	case TOK_WHILE: return parse_while(p);
	case TOK_FOR: return parse_for(p);
	case TOK_INC: case TOK_DEC: return error_prefix_inc(p);
	default: {
		uint32_t node = maybe_inc_dec(p, parse_expr(p, 0));
		consume(p, TOK_SEMCOL, "expected ';' at end of expression");
		return node;
	}
	}
}

uint32_t parse_statement (Parser *p)
{
	if (!enter_depth(p, "expression or statement"))
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);

	uint32_t node = parse_statement_inner(p);
	p->depth--;
	return node;
}
