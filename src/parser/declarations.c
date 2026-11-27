#include "parser/common.h"
#include "parser/operators.h"

#include <string.h>

static uint32_t parse_param (Parser *p, int *typed)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_VAR_DEC, line, col);
	int is_self = 0;

	if (consume(p, TOK_ID, "expected parameter name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
		is_self = p->prev.len == 4 && memcmp(p->prev.str, "self", 4) == 0;
	}

	if (is_self) {
		p->ast.nodes[node].flags |= NODE_FLAG_SELF;
		if (p->curr.type != TOK_COL) {
			p->ast.nodes[node].data_type = TYPE_VOID;
			p->ast.nodes[node].type_node = NO_TYPE_NODE;
			return node;
		}
		error_report(p->err, ERR_ERROR, token_loc(p->curr),
				"'self' has an implicit type and cannot declare one");
	}

	consume(p, TOK_COL, "expected ':' followed by the parameter type");
	*typed = 0;
	if (p->curr.type == TOK_ELLIPSIS) {
		*typed = 1;
		advance(p);
	}
	if (p->curr.type == TOK_AND_A) {
		p->ast.nodes[node].flags |= NODE_FLAG_REF;
		advance(p);
	}
	uint16_t type_node;
	TypeId type = parse_type(p, 0, node, &type_node);
	p->ast.nodes[node].data_type = type;
	p->ast.nodes[node].type_node = type_node;

	if (p->curr.type == TOK_ASSIGN) {
		advance(p);
		uint32_t def = parse_expr(p, 0);
		p->ast.nodes[node].child = def;
	}
	return node;
}

static uint32_t parse_args_dec (Parser *p)
{
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t args = new_node(p, NODE_ARGS_DEC, line, col);
	uint32_t last = NO_NODE;
	int variadic = 0;

	consume(p, TOK_LPAREN, "expected '(' after function name");

	while (p->curr.type == TOK_ID) {
		int typed = 0;
		uint32_t param = parse_param(p, &typed);

		append_child(p, args, &last, param);
		if (typed) p->ast.nodes[args].flags |= NODE_FLAG_VARARG_TYPED;
		if (p->curr.type != TOK_COMMA) break;
		advance(p);
		if (p->curr.type == TOK_ELLIPSIS) {
			variadic = 1;
			advance(p);
			if (p->curr.type != TOK_RPAREN && !p->panic_mode)
				error_report(p->err, ERR_ERROR, token_loc(p->prev),
						"a typed variadic parameter must be named ('name : ...Type')");
			break;
		}
		if (typed && !p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->prev),
					"the variadic parameter must be the last parameter");
	}
	if (last == NO_NODE && p->curr.type == TOK_ELLIPSIS) {
		variadic = 1;
		advance(p);
		if (p->curr.type != TOK_RPAREN && !p->panic_mode)
			error_report(p->err, ERR_ERROR, token_loc(p->prev),
					"a typed variadic parameter must be named ('name : ...Type')");
	}
	if (variadic) p->ast.nodes[args].flags |= NODE_FLAG_VARARG;

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
	uint32_t body = NO_NODE;

	if (p->r2i_mode) {
		consume(p, TOK_SEMCOL, "expected ';' after the imported function declaration");
	} else body = parse_block(p);

	uint32_t last = NO_NODE;
	append_child(p, node, &last, args);
	append_child(p, node, &last, ret);
	if (body != NO_NODE) append_child(p, node, &last, body);
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

static uint32_t parse_field_named (Parser *p, Token name)
{
	uint32_t node = new_node(p, NODE_FIELD_DEC, name.line, name.col);

	p->ast.nodes[node].str = name.str;
	p->ast.nodes[node].len = name.len;
	consume(p, TOK_COL, "expected ':' followed by the field type");
	parse_field_type(p, node);
	consume(p, TOK_SEMCOL, "expected ';' after the field");
	return node;
}

static uint32_t parse_field (Parser *p)
{
	Token name = p->curr;

	if (consume(p, TOK_ID, "expected field name")) name = p->prev;
	return parse_field_named(p, name);
}

static inline int member_named (Token t, const char *name, uint16_t len)
{
	return t.type == TOK_ID && t.len == len && memcmp(t.str, name, len) == 0;
}

static uint32_t parse_special_method (Parser *p, Token name, uint16_t flags)
{
	uint32_t node = new_node(p, NODE_FN_DEC, name.line, name.col);
	p->ast.nodes[node].str = name.str;
	p->ast.nodes[node].len = name.len;
	p->ast.nodes[node].flags = flags;

	uint32_t args = parse_args_dec(p);
	uint32_t ret = new_node(p, NODE_RET_DEC, name.line, name.col);
	p->ast.nodes[ret].data_type = TYPE_VOID;
	p->ast.nodes[ret].type_node = NO_TYPE_NODE;

	if (p->curr.type == TOK_COL) {
		error_report(p->err, ERR_ERROR, token_loc(p->curr),
				"a special class member cannot declare a return type");
		advance(p);
		uint16_t type_node;
		parse_type(p, 0, ret, &type_node);
	}

	uint32_t body = NO_NODE;

	if (p->r2i_mode)
		consume(p, TOK_SEMCOL, "expected ';' after the imported class member declaration");
	else body = parse_block(p);

	uint32_t last = NO_NODE;

	append_child(p, node, &last, args);
	append_child(p, node, &last, ret);
	if (body != NO_NODE) append_child(p, node, &last, body);
	return node;
}

static uint8_t binary_operator (TokenType type)
{
	switch (type)
	{
	case TOK_ADD: return OP_ADD;
	case TOK_SUB: return OP_SUB;
	case TOK_STAR: return OP_MUL;
	case TOK_SLASH: return OP_DIV;
	case TOK_PERCENT: return OP_MOD;
	case TOK_EQ: return OP_EQ;
	case TOK_NE: return OP_NE;
	case TOK_LT: return OP_LT;
	case TOK_LE: return OP_LE;
	case TOK_GT: return OP_GT;
	case TOK_GE: return OP_GE;
	case TOK_AND_A: return OP_AND;
	case TOK_OR_A: return OP_OR;
	case TOK_XOR: return OP_XOR;
	case TOK_LS: return OP_SHL;
	case TOK_RS: return OP_SHR;
	case TOK_ASSIGN: return OP_ASSIGN;
	case TOK_ADD_ASSIGN: return OP_ADD_ASSIGN;
	case TOK_SUB_ASSIGN: return OP_SUB_ASSIGN;
	case TOK_STAR_ASSIGN: return OP_MUL_ASSIGN;
	case TOK_SLASH_ASSIGN: return OP_DIV_ASSIGN;
	case TOK_PERCENT_ASSIGN: return OP_MOD_ASSIGN;
	case TOK_AND_A_ASSIGN: return OP_AND_ASSIGN;
	case TOK_OR_A_ASSIGN: return OP_OR_ASSIGN;
	case TOK_XOR_ASSIGN: return OP_XOR_ASSIGN;
	case TOK_LS_ASSIGN: return OP_SHL_ASSIGN;
	case TOK_RS_ASSIGN: return OP_SHR_ASSIGN;
	default: return OP_NONE;
	}
}

static uint8_t unary_operator (TokenType type)
{
	switch (type)
	{
	case TOK_SUB: return OP_NEG;
	case TOK_NOT_L: return OP_NOT;
	case TOK_NOT_A: return OP_BNOT;
	case TOK_INC: return OP_INC;
	case TOK_DEC: return OP_DEC;
	default: return OP_NONE;
	}
}

static uint32_t count_member_params (Parser *p, uint32_t args)
{
	uint32_t count = 0;

	for (uint32_t a = p->ast.nodes[args].child; a != NO_NODE;
			a = p->ast.nodes[a].next_bro)
		count++;
	return count;
}

static uint8_t operator_canon (Parser *p, TokenType type, int index_op, uint32_t args)
{
	uint32_t count = count_member_params(p, args);
	uint8_t canon;

	if (index_op) return OP_INDEX;
	if (count == 1 && unary_operator(type) != OP_NONE)
		return unary_operator(type);
	if (count == 2 && binary_operator(type) != OP_NONE)
		return binary_operator(type);
	canon = binary_operator(type);
	if (canon == OP_NONE) canon = unary_operator(type);
	return canon;
}

static uint32_t parse_operator (Parser *p)
{
	Token op = p->curr;
	int index_op = op.type == TOK_LBRACE;
	uint32_t node = new_node(p, NODE_FN_DEC, op.line, op.col);
	uint32_t last = NO_NODE;

	if (index_op) {
		advance(p);
		consume(p, TOK_RBRACE, "expected ']' after '[' in the operator name");
	} else advance(p);

	uint32_t args = parse_args_dec(p);

	if (p->curr.type != TOK_COL)
		error_report(p->err, ERR_ERROR, token_loc(p->curr),
				"an operator must declare a return type");
	uint32_t ret = parse_ret_dec(p);
	uint32_t body = NO_NODE;

	if (p->r2i_mode)
		consume(p, TOK_SEMCOL, "expected ';' after the imported operator declaration");
	else body = parse_block(p);

	uint8_t canon = operator_canon(p, op.type, index_op, args);

	p->ast.nodes[node].str = (char *)op_spelling(canon);
	p->ast.nodes[node].len = (uint16_t)strlen(p->ast.nodes[node].str);
	p->ast.nodes[node].op = canon;
	append_child(p, node, &last, args);
	append_child(p, node, &last, ret);
	if (body != NO_NODE) append_child(p, node, &last, body);
	return node;
}

static uint32_t parse_op_member (Parser *p)
{
	Token name = p->curr;

	advance(p);
	if (binary_operator(p->curr.type) != OP_NONE
			|| unary_operator(p->curr.type) != OP_NONE
			|| p->curr.type == TOK_LBRACE)
		return parse_operator(p);
	return parse_field_named(p, name);
}

uint32_t parse_priv_method (Parser *p)
{
	Token kw = p->curr;

	advance(p);
	if (p->curr.type == TOK_ID && member_named(p->curr, "op", 2)) {
		uint32_t member = parse_op_member(p);
		uint8_t type = p->ast.nodes[member].type;
		uint8_t op = p->ast.nodes[member].op;

		if (type != NODE_FN_DEC || op == OP_NONE) {
			error_report(p->err, ERR_ERROR, token_loc(kw),
					"expected 'fn' or an operator after 'priv'");
			return new_node(p, NODE_ERROR, kw.line, kw.col);
		}
		p->ast.nodes[member].flags |= NODE_FLAG_PRIV;
		return member;
	}
	if (p->curr.type != TOK_FN) {
		error_report(p->err, ERR_ERROR, token_loc(kw),
				"expected 'fn' or 'op' after 'priv'");
		p->panic_mode = 1;
		return new_node(p, NODE_ERROR, kw.line, kw.col);
	}

	uint32_t fn = parse_fn_decl(p);

	p->ast.nodes[fn].flags |= NODE_FLAG_PRIV;
	return fn;
}

uint32_t parse_class (Parser *p)
{
	consume(p, TOK_CLASS, "expected 'class'");
	uint16_t line = p->curr.line;
	uint16_t col = p->curr.col;
	uint32_t node = new_node(p, NODE_CLASS_DEC, line, col);
	uint32_t last = NO_NODE;

	if (consume(p, TOK_ID, "expected class name")) {
		p->ast.nodes[node].str = p->prev.str;
		p->ast.nodes[node].len = p->prev.len;
	}
	consume(p, TOK_LKEY, "expected '{' after the class name");

	while (p->curr.type != TOK_RKEY && p->curr.type != TOK_EOF) {
		Token start = p->curr;
		uint32_t member;

		if (p->curr.type == TOK_PRIV) member = parse_priv_method(p);
		else if (p->curr.type == TOK_FN) member = parse_fn_decl(p);
		else if (p->curr.type == TOK_ID
				&& (member_named(p->curr, "init", 4)
					|| member_named(p->curr, "destroy", 7))) {
			Token name = p->curr;
			uint16_t flags = member_named(name, "init", 4)
					? NODE_FLAG_INIT : NODE_FLAG_DESTROY;

			advance(p);
			if (p->curr.type == TOK_LPAREN)
				member = parse_special_method(p, name, flags);
			else member = parse_field_named(p, name);
		}
		else if (p->curr.type == TOK_ID && member_named(p->curr, "op", 2))
			member = parse_op_member(p);
		else if (p->curr.type == TOK_STRUCT || p->curr.type == TOK_UNION)
			member = parse_struct(p, 0);
		else if (p->curr.type == TOK_PACKED) {
			advance(p);
			if (p->curr.type != TOK_STRUCT && p->curr.type != TOK_UNION) {
				error_report(p->err, ERR_ERROR, token_loc(p->curr),
						"expected 'struct' or 'union' after 'packed'");
				p->panic_mode = 1;
				member = new_node(p, NODE_ERROR, p->curr.line, p->curr.col);
			} else member = parse_struct(p, 1);
		} else member = parse_field(p);

		append_child(p, node, &last, member);
		if (p->panic_mode) syncro(p, start);
	}
	consume(p, TOK_RKEY, "expected '}' at the end of the class");
	if (p->curr.type == TOK_SEMCOL) advance(p);
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
