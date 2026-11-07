#include "parser/common.h"

#define MAX_TYPE_RANK 16

enum {
	TS_BUILTIN = 0,
	TS_REF,
	TS_PTR,
	TS_ARRAY,
	TS_SLICE
};

typedef struct TypeSpec {
	struct TypeSpec *base;

	char *str;
	uint32_t dim;

	uint16_t len;
	uint16_t line;
	uint16_t col;

	uint8_t kind;
	TypeId builtin;
} TypeSpec;

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

static TypeSpec *new_spec (Parser *p, uint8_t kind, Token t)
{
	TypeSpec *ts = arena_alloc(p->arena, sizeof(TypeSpec));

	ts->base = NULL;
	ts->str = NULL;
	ts->dim = NO_NODE;
	ts->len = t.len;
	ts->line = t.line;
	ts->col = t.col;
	ts->kind = kind;
	ts->builtin = TYPE_VOID;
	return ts;
}

static TypeSpec *parse_type_spec (Parser *p, int allow_void)
{
	Token t = p->curr;
	TypeSpec *ts;

	if (!enter_depth(p, "type")) return NULL;

	if (t.type == TOK_PTR) {
		advance(p);
		ts = new_spec(p, TS_PTR, t);
		ts->base = parse_type_spec(p, 1);
	} else if (is_type_token(t.type)
			&& (allow_void || t.type != TOK_VOID)) {
		advance(p);
		ts = new_spec(p, TS_BUILTIN, t);
		ts->builtin = tok_to_datatype(t.type);
	} else if (t.type == TOK_ID) {
		advance(p);
		ts = new_spec(p, TS_REF, t);
		ts->str = t.str;
	} else {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(t), "expected type");
		p->panic_mode = 1;
		p->depth--;
		return NULL;
	}

	TypeSpec *first = NULL;
	TypeSpec *last = NULL;
	uint32_t rank = 0;

	while (p->curr.type == TOK_LBRACE) {
		Token open = p->curr;
		uint8_t kind;
		uint32_t dim;

		advance(p);
		if (p->curr.type == TOK_RBRACE) {
			advance(p);
			kind = TS_SLICE;
			dim = NO_NODE;
		} else {
			dim = parse_expr(p, 0);
			consume(p, TOK_RBRACE, "expected ']' after array size");
			kind = TS_ARRAY;
		}

		if (rank >= MAX_TYPE_RANK) {
			if (!p->panic_mode)
				error_report(p->err, ERR_ERROR, token_loc(open),
						"type has too many dimensions (max %d)", MAX_TYPE_RANK);
			p->panic_mode = 1;
			continue;
		}
		rank++;

		TypeSpec *item = new_spec(p, kind, open);
		item->dim = dim;
		if (!first) first = item;
		else last->base = item;
		last = item;
	}
	if (first) {
		last->base = ts;
		ts = first;
	}

	p->depth--;
	return ts;
}

static TypeId make_type_spec (Parser *p, TypeSpec *ts, uint32_t owner, uint16_t *type_node)
{
	uint32_t node;
	TypeId base;
	uint16_t base_node;

	if (ts->kind == TS_BUILTIN) {
		*type_node = NO_TYPE_NODE;
		return ts->builtin;
	}
	if (ts->kind == TS_REF) {
		node = new_node(p, NODE_TYPE_REF, ts->line, ts->col);
		p->ast.nodes[node].str = ts->str;
		p->ast.nodes[node].len = ts->len;
		*type_node = (uint16_t)(node - owner);
		return TYPE_ERROR;
	}

	switch (ts->kind)
	{
	case TS_PTR:
		node = new_node(p, NODE_TYPE_PTR, ts->line, ts->col);
		break;
	case TS_ARRAY:
		node = new_node(p, NODE_TYPE_ARRAY, ts->line, ts->col);
		p->ast.nodes[node].child = ts->dim;
		break;
	default:
		node = new_node(p, NODE_TYPE_SLICE, ts->line, ts->col);
		break;
	}

	base = ts->base ? make_type_spec(p, ts->base, node, &base_node) : TYPE_ERROR;
	if (!ts->base) base_node = NO_TYPE_NODE;
	p->ast.nodes[node].data_type = base;
	p->ast.nodes[node].type_node = base_node;
	*type_node = (uint16_t)(node - owner);
	return TYPE_ERROR;
}

TypeId parse_type (Parser *p, int allow_void, uint32_t owner, uint16_t *type_node)
{
	TypeSpec *ts = parse_type_spec(p, allow_void);

	if (!ts) {
		*type_node = NO_TYPE_NODE;
		return TYPE_ERROR;
	}
	return make_type_spec(p, ts, owner, type_node);
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
		uint32_t child = parse_expr(p, 0);
		p->ast.nodes[node].child = child;
	}

	consume(p, TOK_RPAREN, "expected ')' after sizeof");
	return node;
}
