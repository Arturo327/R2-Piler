#include "parser/common.h"

static TypeId tok_to_datatype (TokenType t)
{
	switch (t)
	{
	case TOK_i8: return TYPE_i8;
	case TOK_u8: return TYPE_u8;
	case TOK_i16: return TYPE_i16;
	case TOK_u16: return TYPE_u16;
	case TOK_i32: return TYPE_i32;
	case TOK_u32: return TYPE_u32;
	case TOK_i64: return TYPE_i64;
	case TOK_u64: return TYPE_u64;
	case TOK_CHAR: return TYPE_CHAR;
	default: return TYPE_VOID;
	}
}

static int is_type_token (TokenType type)
{
	switch (type)
	{
	case TOK_i8: case TOK_u8: case TOK_i16: case TOK_u16:
	case TOK_i32: case TOK_u32: case TOK_i64: case TOK_u64:
	case TOK_CHAR: case TOK_VOID: case TOK_PTR:
		return 1;
	default:
		return 0;
	}
}

static TypeId parse_type_inner (Parser *p, int allow_void, uint32_t owner, uint16_t *type_node)
{
	TokenType t = p->curr.type;
	int valid = (t == TOK_i8 || t == TOK_u8 || t == TOK_i16 || t == TOK_u16
		|| t == TOK_i32 || t == TOK_u32 || t == TOK_i64 || t == TOK_u64
		|| t == TOK_CHAR || (allow_void && t == TOK_VOID));

	*type_node = NO_TYPE_NODE;
	if (t == TOK_PTR) {
		uint16_t line = p->curr.line;
		uint16_t col = p->curr.col;
		advance(p);

		uint32_t ptr = new_node(p, NODE_TYPE_PTR, line, col);
		uint16_t base_node;
		TypeId base = parse_type(p, 1, ptr, &base_node);
		p->ast.nodes[ptr].data_type = base;
		p->ast.nodes[ptr].type_node = base_node;
		*type_node = (uint16_t)(ptr - owner);
		return TYPE_VOID;
	}
	if (valid) {
		TypeId data_type = tok_to_datatype(t);
		advance(p);
		return data_type;
	}

	if (t == TOK_ID) {
		uint32_t ref = new_node(p, NODE_TYPE_REF, p->curr.line, p->curr.col);
		p->ast.nodes[ref].str = p->curr.str;
		p->ast.nodes[ref].len = p->curr.len;
		*type_node = (uint16_t)(ref - owner);
		advance(p);
		return TYPE_ERROR;
	}

	if (!p->panic_mode)
		error_report(p->err, ERR_ERROR, token_loc(p->curr), "expected type");
	p->panic_mode = 1;
	return TYPE_ERROR;
}

TypeId parse_type (Parser *p, int allow_void, uint32_t owner, uint16_t *type_node)
{
	if (!enter_depth(p, "type")) {
		*type_node = NO_TYPE_NODE;
		return TYPE_ERROR;
	}

	TypeId id = parse_type_inner(p, allow_void, owner, type_node);
	p->depth--;
	return id;
}

uint32_t parse_sizeof (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;

	advance(p);
	uint32_t node = new_node(p, NODE_SIZEOF, line, col);
	consume(p, TOK_LPAREN, "expected '(' after sizeof");

	if (is_type_token(p->curr.type)) {
		uint16_t type_node;
		TypeId type = parse_type(p, 1, node, &type_node);
		p->ast.nodes[node].data_type = type;
		p->ast.nodes[node].type_node = type_node;
	} else {
		p->ast.nodes[node].child = parse_expr(p, 0);
	}

	consume(p, TOK_RPAREN, "expected ')' after sizeof");
	return node;
}
