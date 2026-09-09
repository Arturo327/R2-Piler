#include "parser/parser.h"

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

static uint32_t parse_statement (Parser *p)
{
	(void)p;
	return 1;
}

void parse (Parser *p)
{
	advance(p);
	uint32_t last = parse_statement(p);
	p->ast.nodes[0].child = last;

	while (p->curr.type != TOK_EOF) {
		uint32_t decl = parse_statement(p);
		p->ast.nodes[last].next_bro = decl;
		last = decl;
		if (p->panic_mode) syncro(p);
	}
}
