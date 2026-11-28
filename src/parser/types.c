#include "parser/common.h"

#define MAX_TYPE_RANK 16

enum {
	TS_BUILTIN = 0,
	TS_REF,
	TS_PTR,
	TS_CONST,
	TS_ARRAY,
	TS_SLICE,
	TS_FN
};

typedef struct TypeSpec TypeSpec;

typedef struct FnSig {
	TypeSpec **params;
	uint32_t param_count;
	uint32_t param_cap;

	TypeSpec **rets;
	uint32_t ret_count;
	uint32_t ret_cap;

	uint8_t variadic;
} FnSig;

struct TypeSpec {
	struct TypeSpec *base;

	char *str;
	uint32_t dim;

	uint16_t len;
	uint16_t line;
	uint16_t col;

	uint8_t kind;
	TypeId builtin;

	FnSig *sig;
};

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
	case TOK_ANY: return TYPE_ANY;
	default: return TYPE_VOID;
	}
}

static int is_type_token (TokenType type)
{
	switch (type)
	{
	case TOK_i8: case TOK_u8: case TOK_i16: case TOK_u16:
	case TOK_i32: case TOK_u32: case TOK_i64: case TOK_u64:
	case TOK_CHAR: case TOK_VOID: case TOK_PTR: case TOK_ANY:
	case TOK_FN: case TOK_CONST:
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
	ts->sig = NULL;
	return ts;
}

static void append_sig (Parser *p, TypeSpec ***arr, uint32_t *count, uint32_t *cap, TypeSpec *item)
{
	if (!item) return;
	if (*count >= *cap) {
		uint32_t old = *cap;
		*cap = old ? old << 1 : 4;
		*arr = arena_realloc(p->arena, *arr, sizeof(TypeSpec *) * old,
				sizeof(TypeSpec *) * *cap);
	}
	(*arr)[(*count)++] = item;
}

static TypeSpec *parse_type_spec (Parser *p, int allow_void, int bare_fn, int no_suffix);

static TypeSpec *parse_ref_type (Parser *p, int no_suffix)
{
	if (p->curr.type != TOK_AND_A) return parse_type_spec(p, 0, 0, no_suffix);

	Token t = p->curr;
	TypeSpec *ptr = new_spec(p, TS_PTR, t);

	advance(p);
	ptr->base = parse_type_spec(p, 0, 0, no_suffix);
	return ptr;
}

static TypeSpec *parse_fn_param (Parser *p)
{
	if (p->curr.type == TOK_ID) {
		TypeSpec *spec = parse_type_spec(p, 0, 0, 0);
		if (p->curr.type != TOK_COL) return spec;
		advance(p);
	}
	return parse_ref_type(p, 0);
}

static void parse_fn_ret (Parser *p, FnSig *sig)
{
	if (p->curr.type != TOK_COL) return;

	advance(p);
	if (p->curr.type != TOK_LPAREN) {
		TypeSpec *ret = parse_type_spec(p, 1, 0, 0);

		if (ret && ret->kind == TS_BUILTIN && ret->builtin == TYPE_VOID) return;
		append_sig(p, &sig->rets, &sig->ret_count, &sig->ret_cap, ret);
		return;
	}

	advance(p);
	append_sig(p, &sig->rets, &sig->ret_count, &sig->ret_cap,
			parse_type_spec(p, 0, 0, 0));
	if (p->curr.type != TOK_COMMA) {
		consume(p, TOK_RPAREN, "expected ')' after the return type");
		return;
	}
	while (p->curr.type == TOK_COMMA) {
		advance(p);
		append_sig(p, &sig->rets, &sig->ret_count, &sig->ret_cap,
				parse_type_spec(p, 0, 0, 0));
	}
	consume(p, TOK_RPAREN, "expected ')' after the return types");
}

static FnSig *parse_fn_sig (Parser *p, Token fn)
{
	FnSig *sig = arena_alloc(p->arena, sizeof(FnSig));

	sig->params = NULL;
	sig->param_count = 0;
	sig->param_cap = 0;
	sig->rets = NULL;
	sig->ret_count = 0;
	sig->ret_cap = 0;
	sig->variadic = 0;

	if (!consume(p, TOK_LPAREN, "expected '(' after 'fn'")) return sig;
	while (p->curr.type != TOK_RPAREN && p->curr.type != TOK_EOF) {
		if (p->curr.type == TOK_ELLIPSIS) {
			sig->variadic = 1;
			advance(p);
			break;
		}
		append_sig(p, &sig->params, &sig->param_count, &sig->param_cap,
				parse_fn_param(p));
		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}
	consume(p, TOK_RPAREN, "expected ')' after function parameters");
	parse_fn_ret(p, sig);
	(void)fn;
	return sig;
}

static TypeSpec *parse_type_atom (Parser *p, int allow_void, int bare_fn, int no_suffix);

static TypeSpec *parse_const_type (Parser *p, Token kw, int allow_void, int bare_fn, int no_suffix)
{
	if (p->curr.type == TOK_VAR || p->curr.type == TOK_ARR) {
		if (!p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(kw),
					"the 'const' declaration prefix was removed;"
					" write the type as 'var x : const T'");
		p->panic_mode = 1;
		return NULL;
	}
	if (!enter_depth(p, "type")) return NULL;

	TypeSpec *inner = parse_type_atom(p, allow_void, bare_fn, no_suffix);

	p->depth--;
	if (!inner) return NULL;
	TypeSpec *ts = new_spec(p, TS_CONST, kw);

	ts->base = inner;
	return ts;
}

static TypeSpec *parse_type_atom (Parser *p, int allow_void, int bare_fn, int no_suffix)
{
	Token t = p->curr;
	TypeSpec *ts;

	if (t.type == TOK_LPAREN) {
		advance(p);
		ts = parse_type_spec(p, allow_void, bare_fn, no_suffix);
		consume(p, TOK_RPAREN, "expected ')' after type");
	} else if (t.type == TOK_CONST) {
		advance(p);
		return parse_const_type(p, t, allow_void, bare_fn, no_suffix);
	} else if (t.type == TOK_PTR) {
		advance(p);
		ts = new_spec(p, TS_PTR, t);
		ts->base = parse_type_spec(p, 1, 1, no_suffix);
	} else if (t.type == TOK_FN) {
		advance(p);
		ts = new_spec(p, TS_FN, t);
		ts->sig = parse_fn_sig(p, t);
		if (!bare_fn) {
			TypeSpec *ptr = new_spec(p, TS_PTR, t);
			ptr->base = ts;
			ts = ptr;
		}
	} else if (is_type_token(t.type)
			&& (allow_void || t.type != TOK_VOID)) {
		advance(p);
		ts = new_spec(p, TS_BUILTIN, t);
		ts->builtin = tok_to_datatype(t.type);
	} else if (t.type == TOK_ID) {
		advance(p);
		ts = new_spec(p, TS_REF, t);
		int ok;
		uint16_t name_len;
		ts->str = qualify_name(p, t, &name_len, &ok);
		(void)ok;
		ts->len = name_len;
	} else {
		if (!p->panic_mode) {
			if (t.type == TOK_AND_A)
				error_report(p->err, ERR_ERROR, token_loc(t),
						"references are only allowed as function parameter types");
			else error_report(p->err, ERR_ERROR, token_loc(t), "expected type");
		}
		p->panic_mode = 1;
		return NULL;
	}
	return ts;
}

static TypeSpec *parse_type_spec (Parser *p, int allow_void, int bare_fn, int no_suffix)
{
	TypeSpec *ts;

	if (!enter_depth(p, "type")) return NULL;
	ts = parse_type_atom(p, allow_void, bare_fn, no_suffix);
	if (!ts) {
		p->depth--;
		return NULL;
	}

	TypeSpec *first = NULL;
	TypeSpec *last = NULL;
	uint32_t rank = 0;

	while (!no_suffix && p->curr.type == TOK_LBRACE) {
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

static TypeId make_type_spec (Parser *p, TypeSpec *ts, uint32_t owner, uint16_t *type_node);

static uint32_t make_fn_item (Parser *p, TypeSpec *spec)
{
	uint16_t base_node;
	uint32_t node = new_node(p, NODE_FN_PARAM, spec->line, spec->col);
	TypeId base = make_type_spec(p, spec, node, &base_node);

	p->ast.nodes[node].data_type = base;
	p->ast.nodes[node].type_node = base_node;
	return node;
}

static TypeId make_fn_spec (Parser *p, TypeSpec *ts, uint32_t owner, uint16_t *type_node)
{
	uint32_t node = new_node(p, NODE_TYPE_FN, ts->line, ts->col);
	uint32_t last = NO_NODE;

	p->ast.nodes[node].sym = ts->sig->param_count;
	p->ast.nodes[node].u64 = ts->sig->ret_count;
	if (ts->sig->variadic) p->ast.nodes[node].flags |= NODE_FLAG_VARARG;

	for (uint32_t i = 0; i < ts->sig->param_count; i++)
		append_child(p, node, &last, make_fn_item(p, ts->sig->params[i]));
	for (uint32_t i = 0; i < ts->sig->ret_count; i++)
		append_child(p, node, &last, make_fn_item(p, ts->sig->rets[i]));

	*type_node = (uint16_t)(node - owner);
	return TYPE_ERROR;
}

static TypeId make_type_spec (Parser *p, TypeSpec *ts, uint32_t owner, uint16_t *type_node)
{
	if (ts->kind == TS_BUILTIN) {
		*type_node = NO_TYPE_NODE;
		return ts->builtin;
	}
	if (ts->kind == TS_REF) {
		uint32_t node = new_node(p, NODE_TYPE_REF, ts->line, ts->col);
		p->ast.nodes[node].str = ts->str;
		p->ast.nodes[node].len = ts->len;
		*type_node = (uint16_t)(node - owner);
		return TYPE_ERROR;
	}
	if (ts->kind == TS_FN) return make_fn_spec(p, ts, owner, type_node);
	if (ts->kind == TS_CONST) {
		uint32_t node = new_node(p, NODE_TYPE_CONST, ts->line, ts->col);
		uint16_t base_node;
		TypeId base = ts->base ? make_type_spec(p, ts->base, node, &base_node) : TYPE_ERROR;

		if (!ts->base) base_node = NO_TYPE_NODE;
		p->ast.nodes[node].data_type = base;
		p->ast.nodes[node].type_node = base_node;
		*type_node = (uint16_t)(node - owner);
		return TYPE_ERROR;
	}

	uint32_t node;

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

	uint16_t base_node;
	TypeId base = ts->base ? make_type_spec(p, ts->base, node, &base_node) : TYPE_ERROR;

	if (!ts->base) base_node = NO_TYPE_NODE;
	p->ast.nodes[node].data_type = base;
	p->ast.nodes[node].type_node = base_node;
	*type_node = (uint16_t)(node - owner);
	return TYPE_ERROR;
}

TypeId parse_type (Parser *p, int allow_void, uint32_t owner, uint16_t *type_node)
{
	TypeSpec *ts = parse_type_spec(p, allow_void, 0, 0);

	if (!ts) {
		*type_node = NO_TYPE_NODE;
		return TYPE_ERROR;
	}
	return make_type_spec(p, ts, owner, type_node);
}

TypeId parse_new_type (Parser *p, uint32_t owner, uint16_t *type_node)
{
	TypeSpec *ts = parse_type_atom(p, 0, 0, 1);

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

uint32_t parse_alignof (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;

	advance(p);
	uint32_t node = new_node(p, NODE_ALIGNOF, line, col);
	consume(p, TOK_LPAREN, "expected '(' after alignof");

	if (is_type_token(p->curr.type)) {
		uint16_t type_node;
		TypeId type = parse_type(p, 1, node, &type_node);
		p->ast.nodes[node].data_type = type;
		p->ast.nodes[node].type_node = type_node;
	} else {
		uint32_t child = parse_expr(p, 0);
		p->ast.nodes[node].child = child;
	}

	consume(p, TOK_RPAREN, "expected ')' after alignof");
	return node;
}

uint32_t parse_offsetof (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;

	advance(p);
	uint32_t node = new_node(p, NODE_OFFSETOF, line, col);
	consume(p, TOK_LPAREN, "expected '(' after offsetof");

	uint16_t type_node;
	TypeId type = parse_type(p, 1, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;

	consume(p, TOK_COMMA, "expected ',' after the type in offsetof");

	uint32_t seg = new_node(p, NODE_FIELD_INIT, p->curr.line, p->curr.col);
	if (consume(p, TOK_ID, "expected a field name")) {
		p->ast.nodes[seg].str = p->prev.str;
		p->ast.nodes[seg].len = p->prev.len;
	}
	p->ast.nodes[node].child = seg;

	while (p->curr.type == TOK_DOT) {
		advance(p);
		uint32_t next = new_node(p, NODE_FIELD_INIT, p->curr.line, p->curr.col);
		if (consume(p, TOK_ID, "expected a field name after '.'")) {
			p->ast.nodes[next].str = p->prev.str;
			p->ast.nodes[next].len = p->prev.len;
		}
		p->ast.nodes[seg].next_bro = next;
		seg = next;
	}

	consume(p, TOK_RPAREN, "expected ')' after offsetof");
	return node;
}
