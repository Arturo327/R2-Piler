#include "parser/common.h"

static uint32_t parse_primary (Parser *p);

static const int8_t binop_prec[TOK_COUNT] = {
	[TOK_ASSIGN] = 1,
	[TOK_ADD_ASSIGN] = 1, [TOK_SUB_ASSIGN] = 1,
	[TOK_STAR_ASSIGN] = 1, [TOK_SLASH_ASSIGN] = 1,
	[TOK_PERCENT_ASSIGN] = 1, [TOK_AND_A_ASSIGN] = 1,
	[TOK_OR_A_ASSIGN] = 1, [TOK_XOR_ASSIGN] = 1,
	[TOK_LS_ASSIGN] = 1, [TOK_RS_ASSIGN] = 1, [TOK_QUESTION] = 2,
	[TOK_OR_L] = 3, [TOK_AND_L] = 4, [TOK_OR_A] = 5,
	[TOK_XOR] = 6, [TOK_AND_A] = 7, [TOK_EQ] = 8, [TOK_NE] = 8,
	[TOK_GT] = 9, [TOK_LT] = 9,  [TOK_GE] = 9,  [TOK_LE] = 9,
	[TOK_RS] = 10, [TOK_LS] = 10, [TOK_ADD] = 11, [TOK_SUB] = 11,
	[TOK_STAR] = 12, [TOK_SLASH] = 12, [TOK_PERCENT] = 12
};

static const uint8_t binop_node[TOK_COUNT] = {
	[TOK_ASSIGN] = NODE_ASSIGN,
	[TOK_ADD_ASSIGN] = NODE_ADD_ASSIGN, [TOK_SUB_ASSIGN] = NODE_SUB_ASSIGN,
	[TOK_STAR_ASSIGN] = NODE_MUL_ASSIGN, [TOK_SLASH_ASSIGN] = NODE_DIV_ASSIGN,
	[TOK_PERCENT_ASSIGN] = NODE_MOD_ASSIGN, [TOK_AND_A_ASSIGN] = NODE_AND_A_ASSIGN,
	[TOK_OR_A_ASSIGN] = NODE_OR_A_ASSIGN, [TOK_XOR_ASSIGN] = NODE_XOR_ASSIGN,
	[TOK_LS_ASSIGN] = NODE_LS_ASSIGN, [TOK_RS_ASSIGN] = NODE_RS_ASSIGN,
	[TOK_ADD] = NODE_ADD, [TOK_SUB] = NODE_SUB, [TOK_STAR] = NODE_MUL,
	[TOK_SLASH] = NODE_DIV, [TOK_PERCENT] = NODE_MOD, [TOK_AND_A] = NODE_AND_A,
	[TOK_OR_A] = NODE_OR_A, [TOK_XOR] = NODE_XOR, [TOK_RS] = NODE_RS,
	[TOK_LS] = NODE_LS, [TOK_AND_L] = NODE_AND_L, [TOK_OR_L] = NODE_OR_L,
	[TOK_EQ] = NODE_EQ, [TOK_NE] = NODE_NE, [TOK_GT] = NODE_GT,
	[TOK_LT] = NODE_LT, [TOK_GE] = NODE_GE, [TOK_LE] = NODE_LE,
};

static const uint8_t is_compound_assign[TOK_COUNT] = {
	[TOK_ADD_ASSIGN] = 1, [TOK_SUB_ASSIGN] = 1, [TOK_STAR_ASSIGN] = 1,
	[TOK_SLASH_ASSIGN] = 1, [TOK_PERCENT_ASSIGN] = 1, [TOK_AND_A_ASSIGN] = 1,
	[TOK_OR_A_ASSIGN] = 1, [TOK_XOR_ASSIGN] = 1, [TOK_LS_ASSIGN] = 1,
	[TOK_RS_ASSIGN] = 1
};

static uint32_t make_binary_node (Parser *p, uint32_t left, uint32_t right, Token op)
{
	uint32_t node = new_node(p, (NodeType)binop_node[op.type], op.line, op.col);
	uint32_t last = NO_NODE;

	p->ast.nodes[node].len = op.len;
	append_child(p, node, &last, left);
	append_child(p, node, &last, right);
	return node;
}

static uint32_t parse_inc_dec (Parser *p, uint32_t target)
{
	Token op = p->curr;
	NodeType type = op.type == TOK_INC ? NODE_INC : NODE_DEC;
	uint32_t node = new_node(p, type, op.line, op.col);

	p->ast.nodes[node].len = op.len;
	p->ast.nodes[node].child = target;
	advance(p);
	return node;
}

uint32_t maybe_inc_dec (Parser *p, uint32_t node)
{
	Token op = p->curr;

	if (op.type != TOK_INC && op.type != TOK_DEC)
		return node;
	if (p->ast.nodes[node].type == NODE_ID
			|| p->ast.nodes[node].type == NODE_DEREF
			|| p->ast.nodes[node].type == NODE_INDEX
			|| p->ast.nodes[node].type == NODE_DOT)
		return parse_inc_dec(p, node);

	error_report(p->err, ERR_ERROR, token_loc(op),
			"'%s' is only valid as a standalone statement on a variable",
			op.type == TOK_INC ? "++" : "--");
	advance(p);
	return node;
}

static uint32_t parse_named_arg (Parser *p, uint32_t arg)
{
	ASTNode *n = p->ast.nodes + arg;

	if (n->flags & NODE_FLAG_PAREN) return arg;
	if (n->type != NODE_ASSIGN || n->child == NO_NODE) return arg;
	uint32_t name = n->child;
	if (p->ast.nodes[name].type != NODE_ID) return arg;
	uint32_t value = p->ast.nodes[name].next_bro;

	n->type = NODE_NAMED_ARG;
	n->line = p->ast.nodes[name].line;
	n->col = p->ast.nodes[name].col;
	n->str = p->ast.nodes[name].str;
	n->len = p->ast.nodes[name].len;
	n->child = value;
	p->ast.nodes[value].next_bro = NO_NODE;
	return arg;
}

static uint32_t parse_call_args (Parser *p, uint32_t call)
{
	uint32_t last = NO_NODE;
	consume(p, TOK_LPAREN, "expected '('");

	while (p->curr.type != TOK_RPAREN && p->curr.type != TOK_EOF) {
		uint32_t arg = parse_named_arg(p, parse_expr(p, 0));

		if (p->curr.type == TOK_ELLIPSIS) {
			Token dots = p->curr;

			advance(p);
			uint32_t spread = new_node(p, NODE_SPREAD, dots.line, dots.col);

			p->ast.nodes[spread].child = arg;
			p->ast.nodes[spread].len = dots.len;
			arg = spread;
		}
		append_child(p, call, &last, arg);

		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RPAREN, "expected ')' after arguments");
	return call;
}

static uint32_t parse_id_or_call (Parser *p)
{
	Token first = p->curr;
	uint16_t line = first.line;
	uint16_t col = first.col;
	uint16_t len;
	int ok;

	advance(p);
	char *name = qualify_name(p, first, &len, &ok);
	(void)ok;

	if (p->curr.type != TOK_LPAREN) {
		uint32_t node = new_node(p, NODE_ID, line, col);
		p->ast.nodes[node].str = name;
		p->ast.nodes[node].len = len;
		return node;
	}

	uint32_t node = new_node(p, NODE_FN_CALL, line, col);
	p->ast.nodes[node].str = name;
	p->ast.nodes[node].len = len;
	return parse_call_args(p, node);
}

static uint32_t parse_unary (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint16_t len = p->curr.len;
	NodeType type = NODE_NEG;

	if (p->curr.type == TOK_NOT_L) type = NODE_NOT_L;
	else if (p->curr.type == TOK_NOT_A) type = NODE_NOT_A;
	else if (p->curr.type == TOK_AT) type = NODE_DEREF;
	else if (p->curr.type == TOK_DOLLAR) type = NODE_ADDR;

	advance(p);

	uint32_t node = new_node(p, type, line, col);
	p->ast.nodes[node].len = len;
	uint32_t child = parse_primary(p);
	p->ast.nodes[node].child = child;
	return node;
}

static uint32_t parse_new_dims (Parser *p, uint32_t node, uint32_t *last)
{
	uint32_t dims = 0;

	while (p->curr.type == TOK_LBRACE && !p->panic_mode) {
		advance(p);
		if (p->curr.type == TOK_RBRACE) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"expected the allocation size after '['");
			p->panic_mode = 1;
			break;
		}
		uint32_t dim = parse_expr(p, 0);

		consume(p, TOK_RBRACE, "expected ']' after the allocation size");
		append_child(p, node, last, dim);
		dims++;
	}
	return dims;
}

static uint32_t parse_new_alloc (Parser *p, uint32_t node, int *custom)
{
	uint32_t alloc = NO_NODE;

	if (p->curr.type != TOK_LPAREN) return NO_NODE;
	advance(p);
	alloc = parse_expr(p, 0);
	consume(p, TOK_RPAREN, "expected ')' after the allocator expression");
	*custom = 1;
	p->ast.nodes[node].flags |= NODE_FLAG_CUSTOM_ALLOC;
	return alloc;
}

static uint32_t parse_new (Parser *p)
{
	Token kw = p->curr;
	uint32_t node = new_node(p, NODE_NEW, kw.line, kw.col);
	uint32_t last = NO_NODE;
	int custom = 0;

	p->ast.nodes[node].len = kw.len;
	advance(p);
	uint32_t alloc = parse_new_alloc(p, node, &custom);

	uint16_t type_node;
	TypeId base = parse_new_type(p, node, &type_node);

	p->ast.nodes[node].data_type = base;
	p->ast.nodes[node].type_node = type_node;
	if (custom) append_child(p, node, &last, alloc);

	uint32_t dims = parse_new_dims(p, node, &last);

	if (p->curr.type == TOK_LPAREN && !p->panic_mode) {
		if (dims) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"constructor arguments are not supported on array allocations yet");
			uint32_t skip = new_node(p, NODE_FN_CALL, kw.line, kw.col);

			parse_call_args(p, skip);
		} else {
			p->ast.nodes[node].child = NO_NODE;
			p->ast.nodes[node].type = NODE_NEW_CTOR;
			p->ast.nodes[node].u64 = custom ? alloc : NO_NODE;
			parse_call_args(p, node);
			return node;
		}
	}
	p->ast.nodes[node].u64 = dims;
	return node;
}

static uint32_t parse_primary_inner (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node;

	switch (p->curr.type)
	{
	case TOK_SUB: case TOK_NOT_L: case TOK_NOT_A:
	case TOK_AT: case TOK_DOLLAR:
		return parse_unary(p);
	case TOK_NEW:
		return parse_new(p);
	case TOK_SIZEOF:
		return parse_sizeof(p);
	case TOK_ALIGNOF:
		return parse_alignof(p);
	case TOK_OFFSETOF:
		return parse_offsetof(p);
	case TOK_LKEY:
		return parse_brace_init(p);
	case TOK_LPAREN:
		advance(p);
		node = parse_expr(p, 0);
		consume(p, TOK_RPAREN, "expected ')'");
		if (p->ast.nodes[node].type == NODE_ASSIGN)
			p->ast.nodes[node].flags |= NODE_FLAG_PAREN;
		return node;
	case TOK_LIT_i64:
		node = new_node(p, NODE_LIT_i64, line, col);
		p->ast.nodes[node].len = p->curr.len;
		p->ast.nodes[node].u64 = p->curr.u64;
		break;
	case TOK_LIT_u64:
		node = new_node(p, NODE_LIT_u64, line, col);
		p->ast.nodes[node].len = p->curr.len;
		p->ast.nodes[node].u64 = p->curr.u64;
		break;
	case TOK_LIT_CHAR:
		node = new_node(p, NODE_LIT_CHAR, line, col);
		p->ast.nodes[node].len = p->curr.len;
		p->ast.nodes[node].chr = p->curr.chr;
		break;
	case TOK_LIT_STR:
		node = new_node(p, NODE_LIT_STR, line, col);
		p->ast.nodes[node].str = p->curr.str;
		p->ast.nodes[node].len = p->curr.len;
		break;
	case TOK_NULL:
		node = new_node(p, NODE_LIT_NULL, line, col);
		p->ast.nodes[node].len = p->curr.len;
		break;
	case TOK_ID:
		return parse_id_or_call(p);

	default:
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected expression");
		p->panic_mode = 1;
		if (binop_prec[p->curr.type] > 0)
			advance(p);
		return new_node(p, NODE_ERROR, line, col);
	}

	advance(p);
	return node;
}

static uint32_t parse_dot (Parser *p, uint32_t left)
{
	Token dot = p->curr;
	advance(p);

	uint32_t node = new_node(p, NODE_DOT, dot.line, dot.col);
	p->ast.nodes[node].child = left;

	if (consume(p, TOK_ID, "expected a name after '.'")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}
	return node;
}

static uint32_t parse_index (Parser *p, uint32_t left)
{
	Token open = p->curr;
	advance(p);

	uint32_t node = new_node(p, NODE_INDEX, open.line, open.col);
	uint32_t index = parse_expr(p, 0);
	uint32_t last = index;

	p->ast.nodes[node].child = left;
	p->ast.nodes[left].next_bro = index;
	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		uint32_t next = parse_expr(p, 0);

		p->ast.nodes[last].next_bro = next;
		last = next;
	}
	consume(p, TOK_RBRACE, "expected ']' after index");
	return node;
}

static uint32_t parse_indirect_call (Parser *p, uint32_t callee)
{
	uint32_t node = new_node(p, NODE_FN_CALL,
			p->ast.nodes[callee].line, p->ast.nodes[callee].col);

	p->ast.nodes[node].u64 = callee;
	p->ast.nodes[node].len = p->ast.nodes[callee].len;
	p->ast.nodes[node].flags |= NODE_FLAG_INDIRECT;
	return parse_call_args(p, node);
}

uint32_t parse_postfix (Parser *p, uint32_t node)
{
	while (p->curr.type == TOK_DOT || p->curr.type == TOK_LBRACE
			|| (p->curr.type == TOK_LPAREN
			&& p->ast.nodes[node].type != NODE_ERROR)) {
		if (p->curr.type == TOK_DOT) node = parse_dot(p, node);
		else if (p->curr.type == TOK_LPAREN) node = parse_indirect_call(p, node);
		else node = parse_index(p, node);
	}
	return node;
}

static uint32_t parse_reset_atom (Parser *p)
{
	Token tok = p->curr;

	if (tok.type != TOK_ID) {
		error_report(p->err, ERR_ERROR, token_loc(tok),
				"expected a variable, a field or an index as the target of 'reset'");
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, tok.line, tok.col);
	}

	uint16_t len;
	int ok;
	uint32_t node = new_node(p, NODE_ID, tok.line, tok.col);

	advance(p);
	char *name = qualify_name(p, tok, &len, &ok);

	(void)ok;
	p->ast.nodes[node].str = name;
	p->ast.nodes[node].len = len;
	return node;
}

static uint32_t parse_reset_target (Parser *p)
{
	if (p->curr.type == TOK_AT) {
		Token tok = p->curr;
		uint32_t node = new_node(p, NODE_DEREF, tok.line, tok.col);

		advance(p);
		uint32_t child = parse_reset_target(p);

		p->ast.nodes[node].len = tok.len;
		p->ast.nodes[node].child = child;
		return node;
	}
	uint32_t node = parse_reset_atom(p);

	while (!p->panic_mode && (p->curr.type == TOK_DOT || p->curr.type == TOK_LBRACE)) {
		if (p->curr.type == TOK_DOT) node = parse_dot(p, node);
		else node = parse_index(p, node);
	}
	return node;
}

uint32_t parse_reset (Parser *p)
{
	Token kw = p->curr;
	uint32_t node = new_node(p, NODE_RESET, kw.line, kw.col);

	p->ast.nodes[node].len = kw.len;
	advance(p);
	uint32_t target = parse_reset_target(p);

	p->ast.nodes[node].u64 = target;
	if (p->curr.type != TOK_LPAREN) {
		error_report(p->err, ERR_ERROR, token_loc(p->curr),
				"expected '(' and the initializer arguments after the target of 'reset'");
		p->panic_mode = 1;
		return node;
	}
	parse_call_args(p, node);
	consume(p, TOK_SEMCOL, "expected ';' after 'reset'");
	return node;
}

static uint32_t parse_primary (Parser *p)
{
	if (!enter_depth(p, "expression or statement"))
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);

	uint32_t node = parse_postfix(p, parse_primary_inner(p));
	p->depth--;
	return node;
}

static uint32_t parse_ternary (Parser *p, uint32_t cond, Token op)
{
	uint32_t node = new_node(p, NODE_TERNARY, op.line, op.col);
	uint32_t last = NO_NODE;
	uint32_t then_arm = parse_expr(p, 0);
	uint32_t else_arm;

	p->ast.nodes[node].len = op.len;
	consume(p, TOK_COL, "expected ':' in ternary expression");
	else_arm = parse_expr(p, binop_prec[TOK_QUESTION]);
	append_child(p, node, &last, cond);
	append_child(p, node, &last, then_arm);
	append_child(p, node, &last, else_arm);
	return node;
}

uint32_t parse_expr (Parser *p, int min_prec)
{
	uint16_t extra = 0;

	if (!enter_depth(p, "expression or statement"))
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);

	uint32_t left = parse_cast(p);
	int prec = binop_prec[p->curr.type];

	while (prec > 0 && prec >= min_prec && enter_depth(p, "expression or statement")) {
		Token op = p->curr;
		int compound = is_compound_assign[op.type];
		int next_min = (op.type == TOK_ASSIGN || compound) ? prec : prec + 1;

		extra++;
		advance(p);

		if (op.type == TOK_QUESTION) {
			left = parse_ternary(p, left, op);
			prec = binop_prec[p->curr.type];
			continue;
		}

		uint32_t right = parse_expr(p, next_min);
		left = compound ? make_binary_node(p, left, right, op)
				: make_binary_node(p, left, right, op);
		prec = binop_prec[p->curr.type];
	}

	p->depth = (uint16_t)(p->depth - extra - 1);
	return left;
}

uint32_t parse_cast (Parser *p)
{
	uint32_t node = parse_primary(p);
	uint16_t extra = 0;

	while (p->curr.type == TOK_AS && enter_depth(p, "expression or statement")) {
		uint16_t line = p->curr.line;
		uint16_t col = p->curr.col;
		uint32_t cast;

		extra++;
		advance(p);
		cast = new_node(p, NODE_CAST, line, col);
		p->ast.nodes[cast].len = 2;
		uint16_t type_node;
		TypeId type = parse_type(p, 0, cast, &type_node);
		p->ast.nodes[cast].data_type = type;
		p->ast.nodes[cast].type_node = type_node;
		p->ast.nodes[cast].child = node;
		if (p->ast.nodes[node].type == NODE_STRUCT_INIT
				|| (p->ast.nodes[node].type == NODE_ARRAY_INIT
				&& p->ast.nodes[node].child == NO_NODE))
			p->ast.nodes[cast].flags |= NODE_FLAG_LITERAL;
		node = cast;
	}
	p->depth = (uint16_t)(p->depth - extra);
	return node;
}
