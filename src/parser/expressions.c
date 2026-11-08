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

static uint32_t make_compound_assign (Parser *p, uint32_t left, uint32_t right, Token op)
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

static uint32_t parse_call_args (Parser *p, uint32_t call)
{
	uint32_t last = NO_NODE;
	consume(p, TOK_LPAREN, "expected '('");

	while (p->curr.type != TOK_RPAREN && p->curr.type != TOK_EOF) {
		uint32_t arg = parse_expr(p, 0);
		append_child(p, call, &last, arg);

		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RPAREN, "expected ')' after arguments");
	return call;
}

static uint32_t parse_id_or_call (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	char *name = p->curr.str;
	uint16_t len = p->curr.len;
	advance(p);

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
	case TOK_SIZEOF:
		return parse_sizeof(p);
	case TOK_LPAREN:
		advance(p);
		node = parse_expr(p, 0);
		consume(p, TOK_RPAREN, "expected ')'");
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

	p->ast.nodes[node].child = left;
	p->ast.nodes[left].next_bro = index;
	consume(p, TOK_RBRACE, "expected ']' after index");
	return node;
}

static uint32_t parse_primary (Parser *p)
{
	if (!enter_depth(p, "expression or statement"))
		return new_node(p, NODE_ERROR, p->curr.line, p->curr.col);

	uint32_t node = parse_primary_inner(p);
	while (p->curr.type == TOK_DOT || p->curr.type == TOK_LBRACE) {
		if (p->curr.type == TOK_DOT) node = parse_dot(p, node);
		else node = parse_index(p, node);
	}
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
		left = compound ? make_compound_assign(p, left, right, op)
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
		node = cast;
	}
	p->depth = (uint16_t)(p->depth - extra);
	return node;
}
