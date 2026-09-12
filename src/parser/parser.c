#include "parser/parser.h"

#include <stdio.h>

static uint32_t push_ast_node (Parser *p, ASTNode node)
{
	if (p->ast.count >= p->ast.cap) {
		uint32_t old_cap = p->ast.cap;
		p->ast.cap <<= 1;
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
	}
}

static void syncro_advance (Parser *p)
{
	p->panic_mode = 0;

	while (p->curr.type != TOK_EOF) {
		switch (p->curr.type)
		{
		case TOK_SEMCOL:
			advance(p);
			return;

		case TOK_FN: case TOK_VAR: case TOK_IF:
		case TOK_WHILE: case TOK_FOR: case TOK_RET:
		case TOK_RKEY:
			return;

		default: break;
		}

		advance(p);
	}
}

static int same_token_pos (Token a, Token b)
{
	if (a.type != b.type) return 0;
	if (a.line != b.line) return 0;
	if (a.col != b.col) return 0;
	return 1;
}

static void syncro (Parser *p, Token start)
{
	syncro_advance(p);
	if (p->curr.type == TOK_EOF) return;
	if (same_token_pos(p->curr, start)) advance(p);
}

static uint32_t parse_expr (Parser *p, int min_prec);
static uint32_t parse_primary (Parser *p);
static uint32_t parse_statement (Parser *p);
static uint32_t parse_fn_decl (Parser *p);
static uint32_t parse_return (Parser *p);

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
		p->panic_mode = 0;
		return 1;
	}

	if (!p->panic_mode)
		error_report(p->err, ERR_ERROR, token_loc(p->curr), "%s", msg);

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
	NodeType type = NODE_NEG;

	if (p->curr.type == TOK_NOT_L) type = NODE_NOT_L;
	else if (p->curr.type == TOK_NOT_A) type = NODE_NOT_A;

	advance(p);
	uint32_t node = new_node(p, type, line, col);
	uint32_t child = parse_primary(p);
	p->ast.nodes[node].child = child;
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
		return parse_id_or_call(p);

	default:
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected expresion");
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

static uint8_t parse_type (Parser *p, int allow_void)
{
	TokenType t = p->curr.type;
	int valid = (t == TOK_i64 || t == TOK_CHAR || (allow_void && t == TOK_VOID));

	if (!valid) {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected type");
		p->panic_mode = 1;
		return TYPE_VOID;
	}

	uint8_t data_type = tok_to_datatype(t);
	advance(p);
	return data_type;
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
		p->panic_mode = 1;
		return node;
	}
	p->ast.nodes[node].data_type = tok_to_datatype(p->curr.type);
	advance(p);

	if (p->curr.type == TOK_ASSIGN) {
		advance(p);
		uint32_t value = parse_expr(p, 0);
		p->ast.nodes[node].child = value;
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
		Token start = p->curr;
		uint32_t stmt = parse_statement(p);
		append_child(p, block, &last, stmt);
		if (p->panic_mode) syncro(p, start);
	}

	consume(p, TOK_RKEY, "expected '}' for block closing");
	return block;
}

static uint32_t parse_param (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_VAR_DEC, line, col);

	if (consume(p, TOK_ID, "expected parameter name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	consume(p, TOK_COL, "expected ':' followed by the parameter type");
	uint8_t type = parse_type(p, 0);
	p->ast.nodes[node].data_type = type;
	return node;
}

static uint32_t parse_args_dec (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t args = new_node(p, NODE_ARGS_DEC, line, col);
	uint32_t last = NO_NODE;

	consume(p, TOK_LPAREN, "expected '(' after function name");

	while (p->curr.type == TOK_ID) {
		uint32_t param = parse_param(p);
		append_child(p, args, &last, param);

		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RPAREN, "expected ')' after parameters");
	return args;
}

static uint32_t parse_ret_dec (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_RET_DEC, line, col);

	if (p->curr.type != TOK_COL) {
		p->ast.nodes[node].data_type = TYPE_VOID;
		return node;
	}

	advance(p);
	uint8_t type = parse_type(p, 1);
	p->ast.nodes[node].data_type = type;
	return node;
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

static uint32_t parse_fn_decl (Parser *p)
{
	consume(p, TOK_FN, "expected 'fn'");

	uint32_t node = new_node(p, NODE_FN_DEC, p->curr.line, p->curr.col);
	if (consume(p, TOK_ID, "expected function name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	uint32_t args = parse_args_dec(p);
	uint32_t ret = parse_ret_dec(p);
	uint32_t body = parse_block(p);

	uint32_t last = NO_NODE;
	append_child(p, node, &last, args);
	append_child(p, node, &last, ret);
	append_child(p, node, &last, body);
	return node;
}

static inline uint32_t parse_cond (Parser *p)
{
	consume(p, TOK_LPAREN, "expected condition after if statement");
	uint32_t cond = parse_expr(p, 0);
	consume(p, TOK_RPAREN, "expected ')'");
	return cond;
}

static void parse_elif_chain (Parser *p, uint32_t if_node, uint32_t *last)
{
	while (p->curr.type == TOK_ELIF) {
		uint16_t eline = p->curr.line;
		uint16_t ecol = p->curr.col;
		consume(p, TOK_ELIF, "expected 'elif'");

		uint32_t elif_node = new_node(p, NODE_ELIF, eline, ecol);
		uint32_t elif_cond = parse_cond(p);
		uint32_t elif_body = parse_statement(p);

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
	uint32_t else_body = parse_statement(p);

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
	uint32_t cond = parse_cond(p);
	uint32_t body = parse_statement(p);

	uint32_t last = NO_NODE;
	append_child(p, node, &last, cond);
	append_child(p, node, &last, body);
	parse_elif_chain(p, node, &last);

	return node;
}

static uint32_t parse_statement (Parser *p)
{
	switch (p->curr.type)
	{
	case TOK_VAR: return parse_var_dec(p);
	case TOK_LKEY: return parse_block(p);
	case TOK_FN: return parse_fn_decl(p);
	case TOK_RET: return parse_return(p);
	case TOK_IF: return parse_if(p);
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
		Token start = p->curr;
		uint32_t stmt = parse_statement(p);
		append_child(p, 0, &last, stmt);
		if (p->panic_mode) syncro(p, start);
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
	[NODE_ELIF] = "NODE_ELIF",
	[NODE_ELSE] = "NODE_ELSE",
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

static const char *datatype_to_name (DataType t)
{
	switch (t)
	{
	case TYPE_VOID: return "void";
	case TYPE_i64: return "i64";
	case TYPE_CHAR: return "char";
	}
	return "unknown";
}

static void indent (int depth)
{
	while (depth-- > 0) printf("  ");
}

static void dump_node (AST *ast, uint32_t idx, int depth)
{
	ASTNode *n;
	const char *name;

	if (idx == NO_NODE)
		return;

	n = &ast->nodes[idx];
	name = n->type < NODE_COUNT ? node_names[n->type] : "UNKNOWN";
	indent(depth);
	printf("%s [%u:%u]", name, n->line, n->col);

	if (n->type == NODE_LIT_i64)
		printf(" i64=%lld", (long long)n->i64);
	else if (n->type == NODE_LIT_CHAR)
		printf(" char='%c'", n->chr);
	else if (n->type == NODE_LIT_STR || n->type == NODE_ID)
		printf(" str=\"%.*s\"", (int)n->len, n->str);
	else if (n->type == NODE_VAR_DEC || n->type == NODE_FN_DEC || n->type == NODE_FN_CALL) {
		if (n->str) printf(" name=\"%.*s\"", (int)n->len, n->str);
	}

	if (n->type == NODE_VAR_DEC || n->type == NODE_RET_DEC)
		printf(" type=%s", datatype_to_name(n->data_type));

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
