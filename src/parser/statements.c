#include "parser/common.h"

#define FOR_MAX_DIM 8

uint32_t parse_brace_init (Parser *p);

static uint32_t parse_field_init (Parser *p)
{
	consume(p, TOK_DOT, "expected '.'");
	uint32_t node = new_node(p, NODE_FIELD_INIT, p->curr.line, p->curr.col);
	if (consume(p, TOK_ID, "expected a field name after '.'")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}
	consume(p, TOK_ASSIGN, "expected '=' after the field name");

	uint32_t value = parse_expr(p, 0);
	p->ast.nodes[node].child = value;
	return node;
}

enum {
	INIT_POSITIONAL = 0,
	INIT_FIELD,
	INIT_INDEX
};

static int init_item_mode (Parser *p)
{
	if (p->curr.type == TOK_DOT) return INIT_FIELD;
	if (p->curr.type == TOK_LBRACE) return INIT_INDEX;
	return INIT_POSITIONAL;
}

static uint32_t parse_index_init (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_INDEX_INIT, line, col);
	uint32_t first = NO_NODE;
	uint32_t last = NO_NODE;
	uint32_t count = 0;

	while (p->curr.type == TOK_LBRACE && !p->panic_mode) {
		uint32_t dim;

		advance(p);
		dim = parse_expr(p, 0);
		consume(p, TOK_RBRACE, "expected ']' after the initializer index");
		if (last == NO_NODE) first = dim;
		else p->ast.nodes[last].next_bro = dim;
		last = dim;
		count++;
	}
	consume(p, TOK_ASSIGN, "expected '=' after the initializer index");

	uint32_t value = parse_expr(p, 0);

	p->ast.nodes[node].child = first;
	p->ast.nodes[node].sym = count;
	p->ast.nodes[node].u64 = value;
	return node;
}

uint32_t parse_brace_init (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_ARRAY_INIT, line, col);
	uint32_t last = NO_NODE;

	consume(p, TOK_LKEY, "expected '{' for array initializer");
	int mode = init_item_mode(p);
	if (mode == INIT_FIELD) p->ast.nodes[node].type = NODE_STRUCT_INIT;

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		if (init_item_mode(p) != mode) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"cannot mix '.field = value', '[index] = value' and positional initializers");
			p->panic_mode = 1;
			break;
		}
		uint32_t item;
		if (mode == INIT_FIELD) item = parse_field_init(p);
		else if (mode == INIT_INDEX) item = parse_index_init(p);
		else item = parse_expr(p, 0);

		append_child(p, node, &last, item);
		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RKEY, "expected '}' at the end of the array initializer");
	return node;
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
	uint32_t last = NO_NODE;

	if (p->curr.type != TOK_SEMCOL) {
		uint32_t value = parse_expr(p, 0);

		append_child(p, node, &last, value);
		while (p->curr.type == TOK_COMMA && !p->panic_mode) {
			advance(p);
			value = parse_expr(p, 0);
			append_child(p, node, &last, value);
		}
	}

	consume(p, TOK_SEMCOL, "expected ';' after return statement");
	return node;
}

static uint32_t parse_loop_jump (Parser *p, NodeType type, const char *msg)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint64_t level = 1;

	advance(p);
	if (p->curr.type == TOK_LIT_i64 || p->curr.type == TOK_LIT_u64) {
		level = p->curr.u64;
		advance(p);
	}

	uint32_t node = new_node(p, type, line, col);

	p->ast.nodes[node].u64 = level;
	consume(p, TOK_SEMCOL, msg);
	return node;
}

static inline uint32_t parse_cond (Parser *p, const char *msg)
{
	consume(p, TOK_LPAREN, msg);
	uint32_t cond = parse_expr(p, 0);
	consume(p, TOK_RPAREN, "expected ')'");
	return cond;
}

uint32_t parse_body (Parser *p)
{
	uint32_t body = parse_statement(p);

	if (p->ast.nodes[body].next_bro == NO_NODE) return body;

	uint16_t line = p->ast.nodes[body].line;
	uint16_t col = p->ast.nodes[body].col;
	uint32_t block = new_node(p, NODE_BLOCK, line, col);

	p->ast.nodes[block].child = body;
	return block;
}

static uint32_t node_tail (Parser *p, uint32_t node)
{
	uint32_t last = p->ast.nodes[node].child;

	while (p->ast.nodes[last].next_bro != NO_NODE)
		last = p->ast.nodes[last].next_bro;
	return last;
}

void parse_loop_else (Parser *p, uint32_t node)
{
	if (p->curr.type != TOK_ELSE) return;

	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t else_last = NO_NODE;
	uint32_t last = node_tail(p, node);

	consume(p, TOK_ELSE, "expected 'else'");
	uint32_t else_node = new_node(p, NODE_ELSE, line, col);
	uint32_t else_body = parse_body(p);

	append_child(p, node, &last, else_node);
	append_child(p, else_node, &else_last, else_body);
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
	parse_loop_else(p, node);

	return node;
}

static uint32_t parse_clause_body (Parser *p)
{
	uint32_t stmt = parse_statement(p);

	if (p->ast.nodes[stmt].type == NODE_BLOCK) return stmt;

	uint16_t line = p->ast.nodes[stmt].line;
	uint16_t col = p->ast.nodes[stmt].col;
	uint32_t block = new_node(p, NODE_BLOCK, line, col);

	p->ast.nodes[block].child = stmt;
	return block;
}

static uint32_t parse_case (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_CASE, "expected 'case'");

	uint32_t node = new_node(p, NODE_CASE, line, col);
	uint32_t last = NO_NODE;

	consume(p, TOK_LPAREN, "expected '(' after 'case'");
	while (p->curr.type != TOK_RPAREN && p->curr.type != TOK_EOF) {
		uint32_t value = parse_expr(p, 0);

		append_child(p, node, &last, value);
		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}
	consume(p, TOK_RPAREN, "expected ')' after the case values");

	uint32_t body = parse_clause_body(p);

	append_child(p, node, &last, body);
	return node;
}

static uint32_t parse_default (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_DEFAULT, "expected 'default'");

	uint32_t node = new_node(p, NODE_DEFAULT, line, col);
	uint32_t body = parse_clause_body(p);

	p->ast.nodes[node].child = body;
	return node;
}

static uint32_t parse_switch (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_SWITCH, "expected 'switch'");

	uint32_t node = new_node(p, NODE_SWITCH, line, col);
	uint32_t cond = parse_cond(p, "expected condition after switch statement");
	uint32_t last = NO_NODE;

	append_child(p, node, &last, cond);
	consume(p, TOK_LKEY, "expected '{' after the switch condition");
	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		if (p->curr.type == TOK_CASE) {
			append_child(p, node, &last, parse_case(p));
		} else if (p->curr.type == TOK_DEFAULT) {
			append_child(p, node, &last, parse_default(p));
		} else {
			if (!p->panic_mode)
				error_report(p->err, ERR_ERROR, token_loc(p->curr),
						"expected 'case' or 'default'");
			p->panic_mode = 1;
			advance(p);
		}
	}
	consume(p, TOK_RKEY, "expected '}' at the end of the switch");
	return node;
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

static uint32_t parse_multi_assign (Parser *p, uint32_t first)
{
	uint16_t line = p->ast.nodes[first].line;
	uint16_t col = p->ast.nodes[first].col;
	uint32_t node = new_node(p, NODE_MULTI_ASSIGN, line, col);
	uint32_t last = NO_NODE;

	append_child(p, node, &last, first);
	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		uint32_t target = parse_expr(p, 2);

		append_child(p, node, &last, target);
	}

	consume(p, TOK_ASSIGN, "expected '=' after the assignment targets");
	uint32_t rhs = parse_expr(p, 0);

	append_child(p, node, &last, rhs);
	consume(p, TOK_SEMCOL, "expected ';' at the end of the assignment");
	return node;
}

static uint32_t parse_depends (Parser *p)
{
	Token kw = p->curr;
	uint32_t node = NO_NODE;
	uint32_t last = NO_NODE;

	advance(p);
	while (1) {
		if (p->curr.type != TOK_LIT_STR) {
			if (!p->panic_mode)
				error_report(p->err, ERR_ERROR, token_loc(p->curr),
						"expected a library name string after 'depends'");
			p->panic_mode = 1;
			while (p->curr.type != TOK_SEMCOL && p->curr.type != TOK_EOF)
				advance(p);
			consume(p, TOK_SEMCOL, "expected ';' after the library name");
			return node != NO_NODE ? node
					: new_node(p, NODE_ERROR, kw.line, kw.col);
		}
		Token name = p->curr;

		advance(p);
		uint32_t at = new_node(p, NODE_DEPENDS_DEC, kw.line, kw.col);

		p->ast.nodes[at].str = name.str;
		p->ast.nodes[at].len = name.len;
		if (node == NO_NODE) node = at;
		else p->ast.nodes[last].next_bro = at;
		last = at;
		if (p->curr.type != TOK_COMMA || p->panic_mode) break;
		advance(p);
	}
	consume(p, TOK_SEMCOL, "expected ';' after the library name");
	return node;
}

static uint32_t error_const_prefix (Parser *p)
{
	Token kw = p->curr;

	advance(p);
	if (!p->panic_mode)
		error_report(p->err, ERR_ERROR, token_loc(kw),
				"the 'const' declaration prefix was removed;"
				" write the type as 'var x : const T'");
	p->panic_mode = 1;
	return new_node(p, NODE_ERROR, kw.line, kw.col);
}

static uint32_t parse_drop (Parser *p)
{
	Token kw = p->curr;
	uint32_t node = new_node(p, NODE_DROP, kw.line, kw.col);

	p->ast.nodes[node].len = kw.len;
	advance(p);
	uint32_t target = parse_expr(p, 0);

	p->ast.nodes[node].child = target;
	consume(p, TOK_SEMCOL, "expected ';' after 'drop'");
	return node;
}

static uint32_t parse_delete (Parser *p)
{
	Token kw = p->curr;
	uint32_t node = new_node(p, NODE_DELETE, kw.line, kw.col);
	uint32_t last = NO_NODE;
	uint32_t target;

	p->ast.nodes[node].len = kw.len;
	advance(p);
	if (p->curr.type == TOK_LPAREN) {
		advance(p);
		target = parse_expr(p, 0);
		consume(p, TOK_RPAREN, "expected ')' after the deallocator expression");
		if (p->curr.type != TOK_SEMCOL) {
			append_child(p, node, &last, target);
			p->ast.nodes[node].flags |= NODE_FLAG_CUSTOM_ALLOC;
			target = parse_expr(p, 0);
		}
	} else {
		target = parse_expr(p, 0);
	}

	append_child(p, node, &last, target);
	consume(p, TOK_SEMCOL, "expected ';' after 'delete'");
	return node;
}

static uint32_t parse_packed (Parser *p)
{
	advance(p);
	if (p->curr.type != TOK_STRUCT && p->curr.type != TOK_UNION) {
		error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected 'struct' or 'union' after 'packed'");
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);
	}
	return parse_struct(p, 1);
}

static uint32_t parse_expr_statement (Parser *p)
{
	uint32_t node = parse_expr(p, 0);
	if (p->curr.type == TOK_COMMA && !p->panic_mode)
		return parse_multi_assign(p, node);
	node = maybe_inc_dec(p, node);
	consume(p, TOK_SEMCOL, "expected ';' at end of expression");
	return node;
}

uint32_t parse_statement_inner (Parser *p, int root)
{
	switch (p->curr.type)
	{
	case TOK_IMPORT: return parse_import(p, root);
	case TOK_FROM: return parse_from(p, root);
	case TOK_VAR: return parse_decl(p, 0);
	case TOK_CONST: return error_const_prefix(p);
	case TOK_STATIC: return parse_static_decl(p);
	case TOK_MODULE: return parse_module(p);
	case TOK_DEPENDS: return parse_depends(p);
	case TOK_PUB: return parse_pub(p, root);
	case TOK_EXPORT: return parse_export(p, root);
	case TOK_ARR: return parse_decl(p, 1);
	case TOK_LKEY: return parse_block(p);
	case TOK_FN: return parse_fn_decl(p);
	case TOK_EXTERN: return parse_extern_decl(p);
	case TOK_ENUM: return parse_enum(p);
	case TOK_TYPE: return parse_type_dec(p, 0);
	case TOK_NEWTYPE: return parse_type_dec(p, 1);
	case TOK_STRUCT: case TOK_UNION: return parse_struct(p, 0);
	case TOK_CLASS: return parse_class(p);
	case TOK_PRIV: return parse_priv_method(p);
	case TOK_PACKED: return parse_packed(p);
	case TOK_RET: return parse_return(p);
	case TOK_BREAK: return parse_loop_jump(p, NODE_BREAK, "expected ';' after 'break'");
	case TOK_CONTINUE: return parse_loop_jump(p, NODE_CONTINUE, "expected ';' after 'continue'");
	case TOK_IF: return parse_if(p);
	case TOK_WHILE: return parse_while(p);
	case TOK_FOR: return parse_for(p);
	case TOK_SWITCH: return parse_switch(p);
	case TOK_DELETE: return parse_delete(p);
	case TOK_DROP: return parse_drop(p);
	case TOK_RESET: return parse_reset(p);
	case TOK_INC: case TOK_DEC: return error_prefix_inc(p);
	default: return parse_expr_statement(p);
	}
}

uint32_t parse_statement (Parser *p)
{
	if (!enter_depth(p, "expression or statement"))
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);

	uint8_t root = p->at_root;
	p->at_root = 0;
	uint32_t node = parse_statement_inner(p, root);
	p->depth--;
	p->at_root = root;
	return node;
}
