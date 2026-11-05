#include "parser/common.h"

#define MAX_PARSE_DEPTH 1000

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
	parser->depth = 0;

	parser->ast.count = 0;
	parser->ast.cap = 64;
	parser->ast.nodes = arena_alloc(arena, sizeof(ASTNode) * 64);

	parser->prev.type = TOK_NONE;
	parser->curr.type = TOK_NONE;

	ASTNode root = {
		.type = NODE_ROOT,
		.line = 0, .col = 0,
		.child = NO_NODE,
		.next_bro = NO_NODE,
		.sym = NO_NODE,
		.type_node = NO_TYPE_NODE
	};
	(void)push_ast_node(parser, root);
}

void advance (Parser *p)
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

		case TOK_FN: case TOK_VAR: case TOK_ARR: case TOK_IF:
		case TOK_WHILE: case TOK_FOR: case TOK_RET:
		case TOK_EXTERN: case TOK_RKEY:
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

void syncro (Parser *p, Token start)
{
	syncro_advance(p);
	if (p->curr.type == TOK_EOF) return;
	if (same_token_pos(p->curr, start)) advance(p);
}

int enter_depth (Parser *p, const char *what)
{
	if (p->depth >= MAX_PARSE_DEPTH) {
		if (!p->panic_mode) {
			ErrorLoc loc = { p->curr.line, p->curr.col, 1 };
			error_report(p->err, ERR_ERROR, loc, "%s nested too deeply", what);
		}
		p->panic_mode = 1;
		return 0;
	}

	p->depth++;
	return 1;
}

ErrorLoc token_loc (Token t)
{
	ErrorLoc loc = {
		.line = t.line,
		.col = t.col,
		.len = t.len ? t.len : 1
	};
	return loc;
}

int consume (Parser *p, TokenType type, const char *msg)
{
	if (p->curr.type == type) {
		advance(p);
		p->panic_mode = 0;
		return 1;
	}

	if (!p->panic_mode) {
		Token at = p->curr;
		if (at.type == TOK_EOF && p->prev.type != TOK_NONE)
			at = p->prev;
		error_report(p->err, ERR_ERROR, token_loc(at), "%s", msg);
	}

	p->panic_mode = 1;
	return 0;
}

uint32_t new_node (Parser *p, NodeType type, uint16_t line, uint16_t col)
{
	ASTNode node = {0};
	node.type = (uint8_t)type;
	node.line = line;
	node.col = col;
	node.child = NO_NODE;
	node.next_bro = NO_NODE;
	node.sym = NO_NODE;
	node.type_node = NO_TYPE_NODE;
	return push_ast_node(p, node);
}

void append_child (Parser *p, uint32_t parent, uint32_t *last, uint32_t child)
{
	if (*last == NO_NODE) p->ast.nodes[parent].child = child;
	else p->ast.nodes[*last].next_bro = child;
	while (p->ast.nodes[child].next_bro != NO_NODE)
		child = p->ast.nodes[child].next_bro;
	*last = child;
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
