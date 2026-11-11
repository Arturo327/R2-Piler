#include "parser/common.h"

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
	uint16_t type_node;
	TypeId type = parse_type(p, 0, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;
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
	uint16_t type_node;
	TypeId type = parse_type(p, 1, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;

	uint32_t last = NO_NODE;
	while (p->curr.type == TOK_COMMA && !p->panic_mode) {
		advance(p);
		uint16_t iline = p->curr.line;
		uint16_t icol = p->curr.col;
		uint32_t item = new_node(p, NODE_RET_ITEM, iline, icol);
		uint16_t item_node;
		TypeId item_type = parse_type(p, 1, item, &item_node);

		p->ast.nodes[item].data_type = item_type;
		p->ast.nodes[item].type_node = item_node;
		append_child(p, node, &last, item);
	}
	return node;
}

uint32_t parse_fn_decl (Parser *p)
{
	consume(p, TOK_FN, "expected 'fn'");

	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_FN_DEC, line, col);

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

static uint32_t parse_extern_fn (Parser *p)
{
	consume(p, TOK_FN, "expected 'fn' or 'var' after 'extern'");

	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_EXTERN_FN_DEC, line, col);

	if (consume(p, TOK_ID, "expected function name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	uint32_t args = parse_args_dec(p);
	uint32_t ret = parse_ret_dec(p);
	uint32_t last = NO_NODE;

	append_child(p, node, &last, args);
	append_child(p, node, &last, ret);
	consume(p, TOK_SEMCOL, "expected ';' after extern declaration");
	return node;
}

static uint32_t parse_extern_var (Parser *p)
{
	consume(p, TOK_VAR, "expected 'var'");

	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_EXTERN_VAR_DEC, line, col);

	if (consume(p, TOK_ID, "expected variable name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	consume(p, TOK_COL, "expected ':', followed by the type");
	uint16_t type_node;
	TypeId type = parse_type(p, 0, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;

	if (p->curr.type == TOK_ASSIGN) {
		error_report(p->err, ERR_ERROR, token_loc(p->curr),
				"extern variables cannot have an initializer");
		while (p->curr.type != TOK_SEMCOL && p->curr.type != TOK_EOF)
			advance(p);
	}

	consume(p, TOK_SEMCOL, "expected ';' at the end of the declaration");
	return node;
}

uint32_t parse_extern_decl (Parser *p)
{
	consume(p, TOK_EXTERN, "expected 'extern'");
	if (p->curr.type == TOK_VAR) return parse_extern_var(p);
	return parse_extern_fn(p);
}

uint32_t parse_enum (Parser *p)
{
	consume(p, TOK_ENUM, "expected 'enum'");

	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_ENUM_DEC, line, col);

	if (consume(p, TOK_ID, "expected enum name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	if (p->curr.type == TOK_COL) {
		advance(p);
		uint16_t type_node;
		TypeId base = parse_type(p, 0, node, &type_node);
		p->ast.nodes[node].data_type = base;
		p->ast.nodes[node].type_node = type_node;
	} else {
		p->ast.nodes[node].data_type = TYPE_u32;
		p->ast.nodes[node].type_node = NO_TYPE_NODE;
	}

	consume(p, TOK_LKEY, "expected '{' after enum name");

	uint32_t last = NO_NODE;
	while (p->curr.type == TOK_ID) {
		uint32_t var = new_node(p, NODE_ENUM_VAL, p->curr.line, p->curr.col);
		p->ast.nodes[var].str = p->curr.str;
		p->ast.nodes[var].len = p->curr.len;
		advance(p);

		if (p->curr.type == TOK_ASSIGN) {
			advance(p);
			uint32_t value = parse_expr(p, 0);
			p->ast.nodes[var].child = value;
		}
		append_child(p, node, &last, var);

		if (p->curr.type != TOK_COMMA) break;
		advance(p);
	}

	consume(p, TOK_RKEY, "expected '}' at the end of the enum");
	if (p->curr.type == TOK_SEMCOL) advance(p);
	return node;
}

uint32_t parse_type_dec (Parser *p, int is_newtype)
{
	TokenType kw = is_newtype ? TOK_NEWTYPE : TOK_TYPE;
	consume(p, kw, is_newtype ? "expected 'newtype'" : "expected 'type'");

	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, is_newtype ? NODE_NEWTYPE_DEC : NODE_TYPE_DEC, line, col);

	if (consume(p, TOK_ID, "expected type name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}

	consume(p, TOK_ASSIGN, "expected '=' after type name");

	uint16_t type_node;
	TypeId base = parse_type(p, 0, node, &type_node);
	p->ast.nodes[node].data_type = base;
	p->ast.nodes[node].type_node = type_node;

	consume(p, TOK_SEMCOL, "expected ';' after type declaration");
	return node;
}

static int bitfield_short_width (Token t, uint32_t *out)
{
	uint64_t v = 0;

	if (t.type != TOK_ID || t.len < 2) return 0;
	if (t.str[0] != 'u' && t.str[0] != 'i') return 0;

	for (uint16_t i = 1; i < t.len; i++) {
		if (t.str[i] < '0' || t.str[i] > '9') return 0;
		if (v <= 64) v = v * 10 + (uint64_t)(t.str[i] - '0');
	}

	*out = v > 64 ? 65 : (uint32_t)v;
	return 1;
}

static void parse_bitfield_width (Parser *p, uint32_t node)
{
	advance(p);
	if (p->curr.type != TOK_LIT_i64 && p->curr.type != TOK_LIT_u64) {
		error_report(p->err, ERR_ERROR, token_loc(p->curr),
				"bitfield width must be an integer literal");
		p->panic_mode = 1;
		return;
	}

	uint32_t width = p->curr.u64 > 64 ? 65 : (uint32_t)p->curr.u64;
	advance(p);
	if (width < 1 || width > 64) {
		error_report(p->err, ERR_ERROR, token_loc(p->prev),
				"bitfield width must be between 1 and 64");
		p->ast.nodes[node].data_type = TYPE_ERROR;
		p->ast.nodes[node].type_node = NO_TYPE_NODE;
		return;
	}
	p->ast.nodes[node].flags |= NODE_FLAG_BITFIELD | NODE_FLAG_BITFIELD_TYPED;
	p->ast.nodes[node].sym = width;
}

static void parse_field_type (Parser *p, uint32_t node)
{
	uint32_t width;

	if (bitfield_short_width(p->curr, &width)) {
		uint8_t sign = p->curr.str[0] == 'u' ? TYPE_UNTYPED_UINT : TYPE_UNTYPED_INT;

		if (width < 1 || width > 64) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"bitfield width must be between 1 and 64");
			p->ast.nodes[node].data_type = TYPE_ERROR;
		} else {
			p->ast.nodes[node].flags |= NODE_FLAG_BITFIELD;
			p->ast.nodes[node].sym = width;
			p->ast.nodes[node].data_type = sign;
		}
		advance(p);
		return;
	}

	uint16_t type_node;
	TypeId type = parse_type(p, 0, node, &type_node);

	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;
	if (p->curr.type == TOK_COL) parse_bitfield_width(p, node);
}

static uint32_t parse_field (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_FIELD_DEC, line, col);

	if (consume(p, TOK_ID, "expected field name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}
	consume(p, TOK_COL, "expected ':' followed by the field type");
	parse_field_type(p, node);
	consume(p, TOK_SEMCOL, "expected ';' after the field");
	return node;
}

uint32_t parse_struct (Parser *p, int packed)
{
	int is_union = p->curr.type == TOK_UNION;
	uint32_t last = NO_NODE;

	consume(p, is_union ? TOK_UNION : TOK_STRUCT, "expected 'struct' or 'union'");
	uint32_t node = new_node(p, NODE_STRUCT_DEC, p->curr.line, p->curr.col);
	if (is_union) p->ast.nodes[node].flags |= NODE_FLAG_UNION;
	if (packed) p->ast.nodes[node].flags |= NODE_FLAG_PACKED;

	if (p->curr.type == TOK_ID) {
		p->ast.nodes[node].str = p->curr.str;
		p->ast.nodes[node].len = p->curr.len;
		advance(p);
	}
	consume(p, TOK_LKEY, "expected '{' after the struct name");

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		Token start = p->curr;

		if (p->curr.type == TOK_PACKED) {
			error_report(p->err, ERR_ERROR, token_loc(p->curr),
					"a nested struct cannot be declared packed");
			advance(p);
		}

		uint32_t member;
		if (p->curr.type == TOK_STRUCT || p->curr.type == TOK_UNION)
			member = parse_struct(p, packed);
		else member = parse_field(p);

		append_child(p, node, &last, member);
		if (p->panic_mode) syncro(p, start);
	}
	consume(p, TOK_RKEY, "expected '}' at the end of the struct");
	if (p->curr.type == TOK_SEMCOL) advance(p);
	return node;
}
