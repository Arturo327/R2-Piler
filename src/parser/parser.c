#include "parser/parser.h"

#include <stdio.h>

static uint32_t push_ast_node (Parser *p, ASTNode node)
{
	if (p->ast.count >= p->ast.cap) {
		uint32_t old_cap = p->ast.cap;
		p->ast.cap = old_cap ? old_cap << 1 : 128;
		p->ast.nodes = arena_realloc(p->arena, p->ast.nodes,
				old_cap * sizeof(ASTNode), p->ast.cap * sizeof(ASTNode));
	}

	uint32_t idx = p->ast.count++;
	p->ast.nodes[idx] = node;
	return idx;
}

void init_parser (Parser *parser, Lexer *lexer, Arena *arena, ErrorReporter *err)
{
	parser->arena = arena;
	parser->err = err;
	parser->lexer = lexer;
	parser->had_error = 0;
	parser->panic_mode = 0;

	parser->ast.count = 0;
	parser->ast.cap = 64;
	parser->ast.nodes = arena_alloc(arena, sizeof(ASTNode) * 64);

	parser->prev.type = TOK_NONE;
	parser->curr.type = TOK_NONE;

	ASTNode root = {
		.type = NODE_ROOT,
		.line = 0, .col = 0,
		.child = NO_NODE,
		.next_bro = NO_NODE
	};
	(void)push_ast_node(parser, root);
}

static void advance (Parser *p)
{
	p->prev = p->curr;
	while (1) {
		p->curr = get_token(p->lexer);
		if (p->curr.type != TOK_INVALID) break;
		p->had_error = 1;
	}
}

static void syncro (Parser *p)
{
	p->panic_mode = 0;

	while (p->curr.type != TOK_EOF) {
		if (p->prev.type == TOK_SEMCOL)
			return;

		switch (p->curr.type)
		{
		case TOK_FN: case TOK_VAR: case TOK_IF:
		case TOK_WHILE: case TOK_FOR: case TOK_RET:
			return;

		default: break;
		}

		advance(p);
	}
}

static uint32_t parse_expr (Parser *p, int min_prec);
static uint32_t parse_primary (Parser *p);
static uint32_t parse_statement (Parser *p);

static ErrorLoc token_loc (Token t)
{
	ErrorLoc loc = {
		.line = t.line,
		.col = t.col,
		.len = t.len ? t.len : 1
	};
	return loc;
}

static int consume (Parser *p, TokenType type, const char *msg)
{
	if (p->curr.type == type) {
		advance(p);
		return 1;
	}

	if (!p->panic_mode)
		error_report(p->err, ERR_ERROR, token_loc(p->curr), "%s", msg);

	p->had_error = 1;
	p->panic_mode = 1;
	return 0;
}

static uint32_t new_node (Parser *p, NodeType type, uint16_t line, uint16_t col)
{
	ASTNode node = {0};
	node.type = (uint8_t)type;
	node.line = line;
	node.col = col;
	node.child = NO_NODE;
	node.next_bro = NO_NODE;
	return push_ast_node(p, node);
}

static void append_child (Parser *p, uint32_t parent, uint32_t *last, uint32_t child)
{
	if (*last == NO_NODE) p->ast.nodes[parent].child = child;
	else p->ast.nodes[*last].next_bro = child;
	*last = child;
}

static uint8_t tok_to_datatype (TokenType t)
{
	if (t == TOK_i64) return TYPE_i64;
	if (t == TOK_CHAR) return TYPE_CHAR;
	return TYPE_VOID;
}

static const int8_t binop_prec[TOK_COUNT] = {
	[TOK_ASSIGN] = 1,
	[TOK_OR_L] = 2,
	[TOK_AND_L] = 3,
	[TOK_OR_A] = 4,
	[TOK_XOR] = 5,
	[TOK_AND_A] = 6,
	[TOK_EQ] = 7, [TOK_NE] = 7,
	[TOK_GT] = 8, [TOK_LT] = 8,  [TOK_GE] = 8,  [TOK_LE] = 8,
	[TOK_RS] = 9, [TOK_LS] = 9,
	[TOK_ADD] = 10, [TOK_SUB] = 10,
	[TOK_STAR] = 11, [TOK_SLASH] = 11,
};

static const uint8_t binop_node[TOK_COUNT] = {
	[TOK_ASSIGN] = NODE_ASSIGN,
	[TOK_ADD] = NODE_ADD,
	[TOK_SUB] = NODE_SUB,
	[TOK_STAR] = NODE_MUL,
	[TOK_SLASH] = NODE_DIV,
	[TOK_AND_A] = NODE_AND_A,
	[TOK_OR_A] = NODE_OR_A,
	[TOK_XOR] = NODE_XOR,
	[TOK_RS] = NODE_RS,
	[TOK_LS] = NODE_LS,
	[TOK_AND_L] = NODE_AND_L,
	[TOK_OR_L] = NODE_OR_L,
	[TOK_EQ] = NODE_EQ,
	[TOK_NE] = NODE_NE,
	[TOK_GT] = NODE_GT,
	[TOK_LT] = NODE_LT,
	[TOK_GE] = NODE_GE,
	[TOK_LE] = NODE_LE,
};

static uint32_t parse_unary (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	NodeType type = NODE_NEG;

	if (p->curr.type == TOK_NOT_L) type = NODE_NOT_L;
	else if (p->curr.type == TOK_NOT_A) type = NODE_NOT_A;

	advance(p);
	uint32_t node = new_node(p, type, line, col);
	p->ast.nodes[node].child = parse_primary(p);
	return node;
}

static uint32_t parse_primary (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node;

	switch (p->curr.type)
	{
	case TOK_SUB: case TOK_NOT_L: case TOK_NOT_A:
		return parse_unary(p);
	case TOK_LPAREN:
		advance(p);
		node = parse_expr(p, 0);
		consume(p, TOK_RPAREN, "expected ')'");
		return node;
	case TOK_LIT_i64:
		node = new_node(p, NODE_LIT_i64, line, col);
		p->ast.nodes[node].i64 = p->curr.i64;
		break;
	case TOK_LIT_CHAR:
		node = new_node(p, NODE_LIT_CHAR, line, col);
		p->ast.nodes[node].chr = p->curr.chr;
		break;
	case TOK_LIT_STR:
		node = new_node(p, NODE_LIT_STR, line, col);
		p->ast.nodes[node].str = p->curr.str;
		p->ast.nodes[node].len = p->curr.len;
		break;
	case TOK_ID:
		node = new_node(p, NODE_ID, line, col);
		p->ast.nodes[node].str = p->curr.str;
		p->ast.nodes[node].len = p->curr.len;
		break;

	default:
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected expresion");
		p->had_error = 1;
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, line, col);
	}

	advance(p);
	return node;
}

static uint32_t parse_expr (Parser *p, int min_prec)
{
	uint32_t left = parse_primary(p);
	int prec = binop_prec[p->curr.type];

	while (prec > 0 && prec >= min_prec) {
		TokenType op_type = p->curr.type;
		uint16_t line = p->curr.line;
		uint16_t col = p->curr.col;
		advance(p);

		int next_min = (op_type == TOK_ASSIGN) ? prec : prec + 1;
		uint32_t right = parse_expr(p, next_min);

		uint32_t node = new_node(p, (NodeType)binop_node[op_type], line, col);
		uint32_t last = NO_NODE;
		append_child(p, node, &last, left);
		append_child(p, node, &last, right);

		left = node;
		prec = binop_prec[p->curr.type];
	}
	return left;
}

static uint32_t parse_var_dec (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_VAR, "expected 'var'");

	uint32_t node = new_node(p, NODE_VAR_DEC, line, col);
	if (consume(p, TOK_ID, "expected variable name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	consume(p, TOK_COL, "expected ':', followed by the type");

	if (p->curr.type != TOK_i64 && p->curr.type != TOK_CHAR) {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected type");
		p->had_error = 1;
		p->panic_mode = 1;
		return node;
	}
	p->ast.nodes[node].data_type = tok_to_datatype(p->curr.type);
	advance(p);

	if (p->curr.type == TOK_ASSIGN) {
		advance(p);
		p->ast.nodes[node].child = parse_expr(p, 0);
	}

	consume(p, TOK_SEMCOL, "expected ';' at the end of the declaration");
	return node;
}

static uint32_t parse_block (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	consume(p, TOK_LKEY, "expected '{' for block opening");

	uint32_t block = new_node(p, NODE_BLOCK, line, col);
	uint32_t last = NO_NODE;

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		uint32_t stmt = parse_statement(p);
		append_child(p, block, &last, stmt);
		if (p->panic_mode) syncro(p);
	}

	consume(p, TOK_RKEY, "expected '}' for block closing");
	return block;
}

static uint32_t parse_statement (Parser *p)
{
	switch (p->curr.type)
	{
	case TOK_VAR: return parse_var_dec(p);
	case TOK_LKEY: return parse_block(p);
	default: {
		uint32_t node = parse_expr(p, 0);
		consume(p, TOK_SEMCOL, "expected ';' at end of expresion");
		return node;
	}
	}
}

void parse (Parser *p)
{
	advance(p);
	if (p->curr.type == TOK_EOF) return;

	uint32_t last = NO_NODE;
	while (p->curr.type != TOK_EOF) {
		uint32_t stmt = parse_statement(p);
		append_child(p, 0, &last, stmt);
		if (p->panic_mode) syncro(p);
	}
}

// debug shit

static const char *node_names[NODE_COUNT] = {
	[NODE_ERROR] = "NODE_ERROR",
	[NODE_EMPTY] = "NODE_EMPTY",
	[NODE_ASSIGN] = "NODE_ASSIGN",
	[NODE_EQ] = "NODE_EQ",
	[NODE_NE] = "NODE_NE",
	[NODE_GT] = "NODE_GT",
	[NODE_GE] = "NODE_GE",
	[NODE_LT] = "NODE_LT",
	[NODE_LE] = "NODE_LE",
	[NODE_AND_L] = "NODE_AND_L",
	[NODE_OR_L] = "NODE_OR_L",
	[NODE_NOT_L] = "NODE_NOT_L",
	[NODE_LIT_CHAR] = "NODE_LIT_CHAR",
	[NODE_LIT_STR] = "NODE_LIT_STR",
	[NODE_LIT_i64] = "NODE_LIT_i64",
	[NODE_ADD] = "NODE_ADD",
	[NODE_SUB] = "NODE_SUB",
	[NODE_MUL] = "NODE_MUL",
	[NODE_DIV] = "NODE_DIV",
	[NODE_AND_A] = "NODE_AND_A",
	[NODE_OR_A] = "NODE_OR_A",
	[NODE_XOR] = "NODE_XOR",
	[NODE_RS] = "NODE_RS",
	[NODE_LS] = "NODE_LS",
	[NODE_NOT_A] = "NODE_NOT_A",
	[NODE_NEG] = "NODE_NEG",
	[NODE_IF] = "NODE_IF",
	[NODE_WHILE] = "NODE_WHILE",
	[NODE_FOR] = "NODE_FOR",
	[NODE_FN_DEC] = "NODE_FN_DEC",
	[NODE_ARGS_DEC] = "NODE_ARGS_DEC",
	[NODE_RET_DEC] = "NODE_RET_DEC",
	[NODE_RET] = "NODE_RET",
	[NODE_FN_CALL] = "NODE_FN_CALL",
	[NODE_VAR_DEC] = "NODE_VAR_DEC",
	[NODE_ID] = "NODE_ID",
	[NODE_BLOCK] = "NODE_BLOCK",
	[NODE_ROOT] = "NODE_ROOT",
};

static void dump_node (AST *ast, uint32_t idx, int depth)
{
	ASTNode *n;
	const char *name;

	if (idx == NO_NODE)
		return;

	n = &ast->nodes[idx];
	name = n->type < NODE_COUNT ? node_names[n->type] : "UNKNOWN";

	while (depth-- > 0)
		printf("  ");
	printf("%s [%u:%u]", name, n->line, n->col);

	if (n->type == NODE_LIT_i64)
		printf(" i64=%lld", (long long)n->i64);
	else if (n->type == NODE_LIT_CHAR)
		printf(" char='%c'", n->chr);
	else if (n->type == NODE_LIT_STR || n->type == NODE_ID)
		printf(" str=\"%.*s\"", (int)n->len, n->str);
	else if (n->type == NODE_VAR_DEC && n->str)
		printf(" name=\"%.*s\"", (int)n->len, n->str);

	printf("\n");

	dump_node(ast, n->child, depth + 1);
	dump_node(ast, n->next_bro, depth);
}

void dump_ast (AST *ast)
{
	if (ast->count == 0)
		return;
	dump_node(ast, 0, 0);
}
