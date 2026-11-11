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

	uint32_t value = p->curr.type == TOK_LKEY ? parse_brace_init(p) : parse_expr(p, 0);
	p->ast.nodes[node].child = value;
	return node;
}

uint32_t parse_brace_init (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_ARRAY_INIT, line, col);
	uint32_t last = NO_NODE;

	consume(p, TOK_LKEY, "expected '{' for array initializer");
	int desig = p->curr.type == TOK_DOT;
	if (desig) p->ast.nodes[node].type = NODE_STRUCT_INIT;

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		if ((p->curr.type == TOK_DOT) != desig) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"cannot mix '.field = value' and positional initializers");
			p->panic_mode = 1;
			break;
		}
		uint32_t item;
		if (desig) item = parse_field_init(p);
		else item = p->curr.type == TOK_LKEY ? parse_brace_init(p) : parse_expr(p, 0);

		append_child(p, node, &last, item);
		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RKEY, "expected '}' at the end of the array initializer");
	return node;
}

static uint32_t parse_decl_name (Parser *p, int is_arr)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, is_arr ? NODE_ARR_DEC : NODE_VAR_DEC, line, col);

	if (consume(p, TOK_ID, "expected variable name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}
	return node;
}

static void parse_decl_types (Parser *p, uint32_t node)
{
	consume(p, TOK_COL, "expected ':', followed by the type");
	uint16_t type_node;
	TypeId type = parse_type(p, 0, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;
}

static uint32_t parse_decl_type (Parser *p, int is_arr)
{
	uint32_t node = parse_decl_name(p, is_arr);

	parse_decl_types(p, node);
	return node;
}

static uint32_t parse_decl_inner (Parser *p, int is_arr)
{
	uint32_t node = parse_decl_type(p, is_arr);

	if (p->curr.type == TOK_ASSIGN) {
		advance(p);
		uint32_t value = p->curr.type == TOK_LKEY
				? parse_brace_init(p) : parse_expr(p, 0);
		p->ast.nodes[node].child = value;
	}

	return node;
}

static uint32_t parse_decl_list (Parser *p, uint32_t first, int is_arr)
{
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

static uint32_t maybe_multi_decl (Parser *p, uint32_t first)
{
	uint32_t node = first;

	if (p->ast.nodes[first].next_bro == NO_NODE) return first;
	while (p->ast.nodes[node].next_bro != NO_NODE) {
		if (p->ast.nodes[node].child != NO_NODE) return first;
		node = p->ast.nodes[node].next_bro;
	}
	if (p->ast.nodes[node].child == NO_NODE) return first;

	uint32_t rhs = p->ast.nodes[node].child;
	uint16_t line = p->ast.nodes[first].line;
	uint16_t col = p->ast.nodes[first].col;
	uint32_t multi = new_node(p, NODE_MULTI_ASSIGN, line, col);

	p->ast.nodes[node].child = NO_NODE;
	p->ast.nodes[node].next_bro = rhs;
	p->ast.nodes[multi].child = first;
	return multi;
}

static uint32_t parse_decl (Parser *p, int is_arr)
{
	consume(p, is_arr ? TOK_ARR : TOK_VAR,
			is_arr ? "expected 'arr'" : "expected 'var'");
	return maybe_multi_decl(p, parse_decl_list(p, parse_decl_inner(p, is_arr), is_arr));
}

static uint32_t parse_static_decl (Parser *p)
{
	consume(p, TOK_STATIC, "expected 'static'");

	if (p->curr.type != TOK_VAR && p->curr.type != TOK_ARR) {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"expected 'var' or 'arr' after 'static'");
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);
	}

	Token start = p->curr;
	uint32_t first = parse_decl(p, p->curr.type == TOK_ARR);

	if (p->ast.nodes[first].type == NODE_MULTI_ASSIGN) {
		error_report(p->err, ERR_ERROR, token_loc(start),
				"a 'static' declaration cannot initialize several variables");
		return first;
	}

	for (uint32_t node = first; node != NO_NODE; node = p->ast.nodes[node].next_bro)
		p->ast.nodes[node].flags |= NODE_FLAG_STATIC;
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

static uint32_t zero_literal (Parser *p, uint32_t src)
{
	uint16_t line = p->ast.nodes[src].line;
	uint16_t col = p->ast.nodes[src].col;
	uint32_t lit = new_node(p, NODE_LIT_i64, line, col);

	p->ast.nodes[lit].len = 1;
	return lit;
}

static uint32_t copy_id (Parser *p, uint32_t src)
{
	uint16_t line = p->ast.nodes[src].line;
	uint16_t col = p->ast.nodes[src].col;
	uint32_t id = new_node(p, NODE_ID, line, col);

	p->ast.nodes[id].str = p->ast.nodes[src].str;
	p->ast.nodes[id].len = p->ast.nodes[src].len;
	return id;
}

static uint32_t copy_lvalue (Parser *p, uint32_t src)
{
	uint8_t type = p->ast.nodes[src].type;
	uint16_t line = p->ast.nodes[src].line;
	uint16_t col = p->ast.nodes[src].col;
	uint16_t len = p->ast.nodes[src].len;
	uint32_t child = p->ast.nodes[src].child;
	uint32_t copy;

	if (type == NODE_ID) {
		char *str = p->ast.nodes[src].str;

		copy = new_node(p, NODE_ID, line, col);
		p->ast.nodes[copy].str = str;
		p->ast.nodes[copy].len = len;
		return copy;
	}
	if ((type == NODE_DEREF || type == NODE_INDEX) && child != NO_NODE) {
		copy = new_node(p, type, line, col);
		p->ast.nodes[copy].len = len;
		p->ast.nodes[copy].child = child;
		return copy;
	}
	return zero_literal(p, src);
}

static uint32_t copy_for_target (Parser *p, uint32_t node)
{
	if (p->ast.nodes[node].type == NODE_VAR_DEC) return copy_id(p, node);
	return copy_lvalue(p, node);
}

static int for_lvalue (Parser *p, uint32_t node)
{
	uint8_t type = p->ast.nodes[node].type;

	return type == NODE_ID || type == NODE_DEREF || type == NODE_INDEX;
}

static uint32_t copy_index_expr (Parser *p, uint32_t node);

static uint32_t copy_dot (Parser *p, uint32_t node)
{
	uint32_t base = copy_index_expr(p, p->ast.nodes[node].child);

	if (base == NO_NODE) return NO_NODE;
	uint32_t copy = new_node(p, NODE_DOT, p->ast.nodes[node].line, p->ast.nodes[node].col);
	p->ast.nodes[copy].str = p->ast.nodes[node].str;
	p->ast.nodes[copy].len = p->ast.nodes[node].len;
	p->ast.nodes[copy].child = base;
	return copy;
}

static uint32_t copy_index_expr (Parser *p, uint32_t node)
{
	if (p->ast.nodes[node].type == NODE_ID) return copy_id(p, node);
	if (p->ast.nodes[node].type == NODE_DOT) return copy_dot(p, node);
	if (p->ast.nodes[node].type != NODE_INDEX) return NO_NODE;

	uint16_t line = p->ast.nodes[node].line;
	uint16_t col = p->ast.nodes[node].col;
	uint32_t child = p->ast.nodes[node].child;
	uint32_t sub = p->ast.nodes[child].next_bro;
	uint32_t base = copy_index_expr(p, child);

	if (base == NO_NODE) return NO_NODE;

	uint32_t copy = new_node(p, NODE_INDEX, line, col);

	p->ast.nodes[copy].child = base;
	p->ast.nodes[base].next_bro = sub;
	return copy;
}

static uint32_t type_into_expr (Parser *p, uint32_t node)
{
	if (p->ast.nodes[node].type == NODE_TYPE_REF) {
		p->ast.nodes[node].type = NODE_ID;
		return node;
	}
	if (p->ast.nodes[node].type != NODE_TYPE_ARRAY
			|| p->ast.nodes[node].type_node == NO_TYPE_NODE)
		return NO_NODE;

	uint32_t base = type_into_expr(p, node + p->ast.nodes[node].type_node);
	if (base == NO_NODE) return NO_NODE;

	uint32_t dim = p->ast.nodes[node].child;
	p->ast.nodes[node].type = NODE_INDEX;
	p->ast.nodes[node].child = base;
	p->ast.nodes[node].type_node = NO_TYPE_NODE;
	p->ast.nodes[base].next_bro = dim;
	return node;
}

static int for_index_room (Parser *p, uint32_t count)
{
	if (count < FOR_MAX_DIM) return 1;

	error_report(p->err, ERR_ERROR, token_loc(p->curr),
			"too many index variables in the 'for' (max %d)", FOR_MAX_DIM);
	p->panic_mode = 1;
	return 0;
}

static uint32_t for_index_init (Parser *p, uint32_t id)
{
	uint16_t line = p->ast.nodes[id].line;
	uint16_t col = p->ast.nodes[id].col;
	uint16_t len = p->ast.nodes[id].len;
	uint32_t init = new_node(p, NODE_ASSIGN, line, col);
	uint32_t last = NO_NODE;

	if (!for_lvalue(p, id) && !p->panic_mode) {
		ErrorLoc loc = { line, col, len ? len : 1 };

		error_report(p->err, ERR_ERROR, loc,
				"index variable must be an identifier, a dereference or an index expression");
	}
	p->ast.nodes[init].len = 1;
	append_child(p, init, &last, id);
	append_child(p, init, &last, zero_literal(p, id));
	return init;
}

static void for_zero_inits (Parser *p, uint32_t *inits, uint32_t count)
{
	for (uint32_t k = 0; k < count; k++) {
		uint32_t lit = zero_literal(p, inits[k]);

		p->ast.nodes[inits[k]].child = lit;
	}
}

static uint32_t index_chain (Parser *p, uint32_t base, uint32_t idx)
{
	uint16_t line = p->ast.nodes[base].line;
	uint16_t col = p->ast.nodes[base].col;
	uint32_t node = new_node(p, NODE_INDEX, line, col);

	p->ast.nodes[node].child = base;
	p->ast.nodes[base].next_bro = idx;
	return node;
}

static uint32_t parse_for_level (Parser *p, uint32_t loop, uint32_t init, uint32_t expr, uint32_t body)
{
	uint32_t idx = p->ast.nodes[init].type == NODE_VAR_DEC
			? init : p->ast.nodes[init].child;
	uint16_t line = p->ast.nodes[idx].line;
	uint16_t col = p->ast.nodes[idx].col;
	uint32_t len = new_node(p, NODE_DOT, p->ast.nodes[expr].line, p->ast.nodes[expr].col);
	uint32_t cmp = new_node(p, NODE_LT, line, col);
	uint32_t inc = new_node(p, NODE_INC, line, col);
	uint32_t clast = NO_NODE;
	uint32_t last = NO_NODE;

	p->ast.nodes[len].str = "len";
	p->ast.nodes[len].len = 3;
	p->ast.nodes[len].child = expr;

	p->ast.nodes[cmp].len = 1;
	append_child(p, cmp, &clast, copy_for_target(p, idx));
	append_child(p, cmp, &clast, len);

	uint32_t inc_id = copy_for_target(p, idx);
	p->ast.nodes[inc].len = 2;
	p->ast.nodes[inc].child = inc_id;

	append_child(p, loop, &last, init);
	append_child(p, loop, &last, cmp);
	append_child(p, loop, &last, inc);
	append_child(p, loop, &last, body);
	return loop;
}

static uint32_t for_index_expr (Parser *p, uint32_t *inits, uint32_t k, uint32_t arr)
{
	uint32_t node = arr;

	if (k) {
		uint32_t copy = copy_index_expr(p, arr);
		if (copy != NO_NODE) node = copy;
	}

	for (uint32_t d = 0; d < k; d++) {
		uint32_t init = inits[d];
		uint32_t idx = p->ast.nodes[init].type == NODE_VAR_DEC
				? init : p->ast.nodes[init].child;

		node = index_chain(p, node, copy_for_target(p, idx));
	}
	return node;
}

static uint32_t parse_for_range (Parser *p, uint32_t node, uint32_t *inits, uint32_t count, uint32_t arr)
{
	uint16_t line = p->ast.nodes[node].line;
	uint16_t col = p->ast.nodes[node].col;
	uint32_t exprs[FOR_MAX_DIM];
	uint32_t body;

	for (uint32_t k = 0; k < count; k++)
		exprs[k] = for_index_expr(p, inits, k, arr);

	consume(p, TOK_RPAREN, "expected ')' after the array");
	body = parse_body(p);
	for (uint32_t k = count; k-- > 0; ) {
		uint32_t loop = k == 0 ? node : new_node(p, NODE_FOR, line, col);

		body = parse_for_level(p, loop, inits[k], exprs[k], body);
	}
	return node;
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

static uint32_t parse_for_tail (Parser *p, uint32_t node, uint32_t beg)
{
	uint32_t decls = NO_NODE;

	if (p->ast.nodes[beg].next_bro != NO_NODE) {
		decls = beg;
		beg = new_node(p, NODE_EMPTY, p->ast.nodes[node].line, p->ast.nodes[node].col);
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

static uint32_t parse_for_names_range (Parser *p, uint32_t node, uint32_t first)
{
	uint32_t inits[FOR_MAX_DIM];
	uint32_t count = 0;

	inits[count++] = first;
	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		if (!for_index_room(p, count)) break;
		inits[count++] = parse_decl_name(p, 0);
	}

	consume(p, TOK_COL, "expected ':' before the array");
	for (uint32_t k = 0; k < count; k++)
		p->ast.nodes[inits[k]].data_type = TYPE_i64;
	for_zero_inits(p, inits, count);
	return parse_for_range(p, node, inits, count, parse_expr(p, 0));
}

static uint32_t parse_for_more_types (Parser *p, uint32_t *inits, uint32_t count)
{
	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		if (!for_index_room(p, count)) break;
		inits[count++] = parse_decl_type(p, 0);
	}
	return count;
}

static uint32_t parse_for_decl_tail (Parser *p, uint32_t node, uint32_t *inits, uint32_t count)
{
	if (p->curr.type == TOK_ASSIGN) {
		advance(p);
		uint32_t value = parse_expr(p, 0);

		p->ast.nodes[inits[count - 1]].child = value;
	}
	for (uint32_t k = 1; k < count; k++)
		p->ast.nodes[inits[k - 1]].next_bro = inits[k];
	return parse_for_tail(p, node, parse_decl_list(p, inits[0], 0));
}

static uint32_t parse_for_var (Parser *p, uint32_t node)
{
	uint32_t inits[FOR_MAX_DIM];
	uint32_t count = 0;

	consume(p, TOK_VAR, "expected 'var'");
	uint32_t last = parse_decl_name(p, 0);
	if (p->curr.type == TOK_COMMA && !p->panic_mode)
		return parse_for_names_range(p, node, last);

	parse_decl_types(p, last);
	inits[count++] = last;
	count = parse_for_more_types(p, inits, count);
	last = inits[count - 1];

	if (p->curr.type == TOK_COL && !p->panic_mode) {
		advance(p);
		for_zero_inits(p, inits, count);
		return parse_for_range(p, node, inits, count, parse_expr(p, 0));
	}
	uint16_t type_node = p->ast.nodes[last].type_node;

	if ((p->curr.type == TOK_RPAREN || p->curr.type == TOK_DOT)
			&& !p->panic_mode && type_node != NO_TYPE_NODE) {
		uint32_t arr = type_into_expr(p, last + type_node);

		if (arr != NO_NODE) {
			arr = parse_postfix(p, arr);
			p->ast.nodes[last].type_node = NO_TYPE_NODE;
			p->ast.nodes[last].data_type = TYPE_i64;
			for_zero_inits(p, inits, count);
			return parse_for_range(p, node, inits, count, arr);
		}
	}
	return parse_for_decl_tail(p, node, inits, count);
}

static uint32_t parse_for_ident_range (Parser *p, uint32_t node, uint32_t first)
{
	uint32_t targets[FOR_MAX_DIM];
	uint32_t inits[FOR_MAX_DIM];
	uint32_t count = 0;

	targets[count++] = first;
	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		if (!for_index_room(p, count)) break;
		targets[count++] = parse_expr(p, 0);
	}
	if (p->curr.type != TOK_COL) {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"expected ':' after the index list in 'for'");
		while (p->curr.type != TOK_SEMCOL && p->curr.type != TOK_RPAREN
				&& p->curr.type != TOK_EOF)
			advance(p);
		if (p->curr.type == TOK_SEMCOL) {
			advance(p);
			return parse_for_tail(p, node, first);
		}
		if (p->curr.type == TOK_RPAREN) advance(p);
		return node;
	}
	advance(p);
	for (uint32_t k = 0; k < count; k++)
		inits[k] = for_index_init(p, targets[k]);
	return parse_for_range(p, node, inits, count, parse_expr(p, 0));
}

static uint32_t parse_for_expr (Parser *p, uint32_t node)
{
	uint32_t beg = parse_expr(p, 0);

	if (!p->panic_mode && (p->curr.type == TOK_COL || p->curr.type == TOK_COMMA))
		return parse_for_ident_range(p, node, beg);

	consume(p, TOK_SEMCOL, "expected ';' between for expressions");
	return parse_for_tail(p, node, beg);
}

static uint32_t parse_for (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_FOR, "expected 'for'");
	uint32_t node = new_node(p, NODE_FOR, line, col);

	consume(p, TOK_LPAREN, "expected '('");
	if (p->curr.type == TOK_VAR && !p->panic_mode)
		return parse_for_var(p, node);
	if (p->curr.type == TOK_ARR && !p->panic_mode)
		return parse_for_tail(p, node, parse_decl(p, 1));
	if (p->curr.type == TOK_SEMCOL && !p->panic_mode) {
		uint32_t e = new_node(p, NODE_EMPTY, p->curr.line, p->curr.col);
		advance(p);
		return parse_for_tail(p, node, e);
	}
	return parse_for_expr(p, node);
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

static uint32_t parse_statement_inner (Parser *p)
{
	switch (p->curr.type)
	{
	case TOK_VAR: return parse_decl(p, 0);
	case TOK_STATIC: return parse_static_decl(p);
	case TOK_ARR: return parse_decl(p, 1);
	case TOK_LKEY: return parse_block(p);
	case TOK_FN: return parse_fn_decl(p);
	case TOK_EXTERN: return parse_extern_decl(p);
	case TOK_ENUM: return parse_enum(p);
	case TOK_TYPE: return parse_type_dec(p, 0);
	case TOK_NEWTYPE: return parse_type_dec(p, 1);
	case TOK_STRUCT: case TOK_UNION: return parse_struct(p, 0);
	case TOK_PACKED: {
		advance(p);
		if (p->curr.type != TOK_STRUCT && p->curr.type != TOK_UNION) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"expected 'struct' or 'union' after 'packed'");
			p->panic_mode = 1;
			return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);
		}
		return parse_struct(p, 1);
	}
	case TOK_RET: return parse_return(p);
	case TOK_IF: return parse_if(p);
	case TOK_WHILE: return parse_while(p);
	case TOK_FOR: return parse_for(p);
	case TOK_SWITCH: return parse_switch(p);
	case TOK_INC: case TOK_DEC: return error_prefix_inc(p);
	default: {
		uint32_t node = parse_expr(p, 0);
		if (p->curr.type == TOK_COMMA && !p->panic_mode)
			return parse_multi_assign(p, node);
		node = maybe_inc_dec(p, node);
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
