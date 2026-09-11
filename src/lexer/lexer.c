#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "lexer/lexer.h"

void init_lexer (Lexer *lexer, char *src, Arena *arena, ErrorReporter *err)
{
	lexer->line = 1;
	lexer->cursor = src;
	lexer->line_start = src;
	lexer->arena = arena;
	lexer->err = err;
}

static inline ErrorLoc loc_here (Lexer *l)
{
	ErrorLoc loc = {
		.line = l->line,
		.col = (int)(l->cursor - l->line_start) + 1,
		.len = 1
	};
	return loc;
}

static inline ErrorLoc loc_at (int line, int col, int len)
{
	ErrorLoc loc = {
		.line = line,
		.col = col,
		.len = len
	};
	return loc;
}

static inline Token make_token (TokenType type, char *text, size_t len, uint16_t line, uint16_t col)
{
	Token token = {
		.type = type,
		.line = line,
		.len = len,
		.col = col,
		.str = text
	};
	return token;
}

static void skip_comment (Lexer *l)
{
	l->cursor += 2;
	while (*l->cursor != '\0') {
		if (*l->cursor == '\n') {
			l->line++;
			l->cursor++;
			l->line_start = l->cursor;
			return;
		}
		l->cursor++;
	}
}

static void skip_whitespace (Lexer *l)
{
	while (1) {
		char c = *l->cursor;
		if (c == ' ' || c == '\t' || c == '\r') {
			l->cursor++;
		} else if (c == '\n') {
			l->cursor++;
			l->line++;
			l->line_start = l->cursor;
		} else if (c == '/' && *(l->cursor + 1) == '/') {
			skip_comment(l);
		} else break;
	}
}

static Token handle_num_literal (Lexer *l)
{
	char *start = l->cursor;
	int start_line = l->line;
	int start_col = (int)(start - l->line_start) + 1;
	int64_t value = 0;
	int overflow = 0;

	while (isdigit((unsigned char)*l->cursor)) {
		int digit = *l->cursor - '0';
		if (!overflow) {
			if (value > (INT64_MAX - digit) / 10) overflow = 1;
			else value = value * 10 + digit;
		}
		l->cursor++;
	}

	if (overflow) {
		error_report(l->err, ERR_ERROR, loc_at(start_line, start_col,
				(int)(l->cursor - start)), "literal integer out of range");
		return make_token(TOK_INVALID, NULL, l->cursor - start, start_line, start_col);
	}

	Token token = {
		.type = TOK_LIT_i64,
		.line = start_line,
		.i64 = value,
		.len = (uint16_t)(l->cursor - start),
		.col = start_col
	};
	return token;
}

static int decode_escape (ErrorReporter *err, char c, char *out, ErrorLoc loc)
{
	switch (c)
	{
	case 'n':  *out = '\n'; break;
	case 't':  *out = '\t'; break;
	case 'r':  *out = '\r'; break;
	case '0':  *out = '\0'; break;
	case '\\': *out = '\\'; break;
	case '\'': *out = '\''; break;
	case '"':  *out = '"'; break;
	default:
		if (isprint((unsigned char)c))
			error_report(err, ERR_ERROR, loc, "unknown escape sequence '\\%c'", c);
		else
			error_report(err, ERR_ERROR, loc, "unknown escape sequence '\\x%02X'", (unsigned char)c);
		*out = c;
		return 0;
	}
	return 1;
}

static char *string_literal_find_end (char *a)
{
	while (*a != '"' && *a != '\0' && *a != '\n') {
		if (*a == '\\' && *(a + 1) != '\0') {
			a += 2;
			continue;
		}
		a++;
	}
	return a;
}

static Token string_literal_decode (Lexer *l, char *end, int start_line, int start_col)
{
	char *str = arena_alloc(l->arena, (size_t)(end - l->cursor) + 1);
	size_t str_len = 0;

	while (l->cursor < end) {
		char c = *l->cursor++;

		if (c != '\\') {
			str[str_len++] = c;
			continue;
		}

		ErrorLoc loc = loc_at(l->line, (int)(l->cursor - l->line_start), 2);
		char escaped = *l->cursor++;
		if (escaped == '\n') {
			l->line++;
			l->line_start = l->cursor;
			continue;
		}

		char decoded;
		if (!decode_escape(l->err, escaped, &decoded, loc))
			return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
		str[str_len++] = decoded;
	}

	str[str_len] = '\0';
	return make_token(TOK_LIT_STR, str, str_len, start_line, start_col);
}

static Token handle_string_literal (Lexer *l)
{
	char *start = l->cursor++;
	int start_line = l->line;
	int start_col = (int)(start - l->line_start) + 1;

	char *end = string_literal_find_end(l->cursor);
	if (*end != '"') {
		for (char *c = l->cursor; c < end; c++) {
			if (c[0] == '\\' && c[1] == '\n') {
				l->line++;
				l->line_start = c + 2;
				c++;
			}
		}
		error_report(l->err, ERR_ERROR, loc_at(start_line, start_col,
				(int)(end - start)), "string literal is not closed");
		l->cursor = end;
		return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
	}

	Token token = string_literal_decode(l, end, start_line, start_col);
	l->cursor = end + 1;
	return token;
}

static char handle_char_literal_next_char (Lexer *l, char *chr)
{
	char c = *l->cursor;
	if (c == '\n') {
		error_report(l->err, ERR_ERROR, loc_here(l), "newline in char literal");
		return 0;
	}
	if (c == '\\') {
		l->cursor++;
		if (*l->cursor == '\0') {
			error_report(l->err, ERR_ERROR, loc_here(l), "incomplete escape sequence");
			return 0;
		}
		if (!decode_escape(l->err, *l->cursor, &c, loc_at(l->line, (int)(l->cursor - l->line_start), 2)))
			return 0;
	}
	l->cursor++;
	*chr = c;
	return 1;
}

static int char_literal_scan_extra (Lexer *l)
{
	int multichr = 0;
	while (*l->cursor != '\0' && *l->cursor != '\'' && *l->cursor != '\n') {
		multichr = 1;
		if (*l->cursor == '\\' && *(l->cursor + 1) != '\0' && *(l->cursor + 1) != '\n')
			l->cursor += 2;
		else l->cursor++;
	}
	return multichr;
}

static Token handle_char_literal (Lexer *l)
{
	char *start = l->cursor++;
	char *line_start = l->line_start;
	int start_line = l->line;
	int start_col = (int)(start - line_start) + 1;

	if (*l->cursor == '\'') {
		error_report(l->err, ERR_ERROR, loc_at(start_line, start_col, 2),
				"empty char literal");
		l->cursor++;
		return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
	}

	char chr;
	if (!handle_char_literal_next_char(l, &chr)) {
		char_literal_scan_extra(l);
		if (*l->cursor == '\'')
			l->cursor++;
		return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
	}

	int multichr = char_literal_scan_extra(l);

	if (*l->cursor != '\'') {
		error_report(l->err, ERR_ERROR, loc_at(start_line, start_col,
				(int)(l->cursor - start)), "char literal is not closed");
		return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
	}
	l->cursor++;

	if (multichr) {
		error_report(l->err, ERR_ERROR, loc_at(start_line, start_col,
				(int)(l->cursor - start)), "char literal must contain exactly one character");
		return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
	}

	Token token = {
		.type = TOK_LIT_CHAR,
		.line = start_line,
		.chr = chr,
		.col = start_col,
		.len = (uint16_t)(l->cursor - start)
	};
	return token;
}

typedef struct Keyword {
	const char *name;
	const TokenType type;
} Keyword;

typedef struct KeywordKey {
	const char *str;
	size_t len;
} KeywordKey;

// WARNING: order must be: first length, then alphabetically.
static const Keyword keywords[] = {
	{"fn", TOK_FN},
	{"if", TOK_IF},
	{"for", TOK_FOR},
	{"i64", TOK_i64},
	{"var", TOK_VAR},
	{"char", TOK_CHAR},
	{"elif", TOK_ELIF},
	{"else", TOK_ELSE},
	{"void", TOK_VOID},
	{"while", TOK_WHILE},
	{"return", TOK_RET}
};

static int cmp_keywords (const void *a, const void *b)
{
	const KeywordKey *key = a;
	const Keyword *kw = b;
	size_t kw_len = strlen(kw->name);

	if (key->len != kw_len)
		return key->len < kw_len ? -1 : 1;

	return memcmp(key->str, kw->name, key->len);
}

static TokenType id_keyword (char *str, size_t length)
{
	KeywordKey key = { .str = str, .len = length };
	const Keyword *kw = bsearch(&key, keywords, sizeof(keywords) / sizeof(keywords[0]),
			sizeof *keywords, cmp_keywords);
	return kw ? kw->type : TOK_ID;
}

typedef struct SimpleOp {
	char ch;
	TokenType type;
} SimpleOp;

static const SimpleOp simple_ops[] = {
	{'+', TOK_ADD}, {'-', TOK_SUB}, {'*', TOK_STAR}, {'/', TOK_SLASH},
	{'^', TOK_XOR}, {';', TOK_SEMCOL}, {':', TOK_COL}, {',', TOK_COMMA},
	{'~', TOK_NOT_A}, {'(', TOK_LPAREN}, {')', TOK_RPAREN},
	{'[', TOK_LBRACE}, {']', TOK_RBRACE}, {'{', TOK_LKEY}, {'}', TOK_RKEY},
};

static int find_simple_op (char c, TokenType *out)
{
	for (size_t i = 0; i < sizeof(simple_ops) / sizeof(simple_ops[0]); i++) {
		if (simple_ops[i].ch == c) {
			*out = simple_ops[i].type;
			return 1;
		}
	}
	return 0;
}

static Token handle_compound_op (Lexer *l, char c, int start_col)
{
	switch (c)
	{
	case '=':
		if (*l->cursor == '=') {
			l->cursor++;
			return make_token(TOK_EQ, NULL, 2, l->line, start_col);
		}
		return make_token(TOK_ASSIGN, NULL, 1, l->line, start_col);
	case '!':
		if (*l->cursor == '=') {
			l->cursor++;
			return make_token(TOK_NE, NULL, 2, l->line, start_col);
		}
		return make_token(TOK_NOT_L, NULL, 1, l->line, start_col);
	case '>':
		if (*l->cursor == '>') {
			l->cursor++;
			return make_token(TOK_RS, NULL, 2, l->line, start_col);
		} else if (*l->cursor == '=') {
			l->cursor++;
			return make_token(TOK_GE, NULL, 2, l->line, start_col);
		}
		return make_token(TOK_GT, NULL, 1, l->line, start_col);
	case '<':
		if (*l->cursor == '<') {
			l->cursor++;
			return make_token(TOK_LS, NULL, 2, l->line, start_col);
		} else if (*l->cursor == '=') {
			l->cursor++;
			return make_token(TOK_LE, NULL, 2, l->line, start_col);
		}
		return make_token(TOK_LT, NULL, 1, l->line, start_col);
	case '&':
		if (*l->cursor == '&') {
			l->cursor++;
			return make_token(TOK_AND_L, NULL, 2, l->line, start_col);
		}
		return make_token(TOK_AND_A, NULL, 1, l->line, start_col);
	case '|':
		if (*l->cursor == '|') {
			l->cursor++;
			return make_token(TOK_OR_L, NULL, 2, l->line, start_col);
		}
		return make_token(TOK_OR_A, NULL, 1, l->line, start_col);
	default:
		return make_token(TOK_INVALID, NULL, 0, l->line, start_col);
	}
}

static Token handle_symbols (Lexer *l)
{
	int sym_line = l->line;
	int sym_col = (int)(l->cursor - l->line_start) + 1;
	char c = *l->cursor++;
	TokenType simple;

	if (find_simple_op(c, &simple)) return make_token(simple, NULL, 1, l->line, sym_col);
	if (strchr("=!><&|", c)) return handle_compound_op(l, c, sym_col);

	if (isprint((unsigned char)c))
		error_report(l->err, ERR_ERROR, loc_at(sym_line, sym_col, 1),
				"character '%c' is not valid", c);
	else
		error_report(l->err, ERR_ERROR, loc_at(sym_line, sym_col, 1),
				"character '\\x%02X' is not valid", (unsigned char)c);
	return make_token(TOK_INVALID, NULL, 0, l->line, sym_col);
}

Token get_token (Lexer *l)
{
	skip_whitespace(l);
	char c = *l->cursor;

	if (c == '\0')
		return make_token(TOK_EOF, l->cursor, 0, l->line, (uint16_t)(l->cursor - l->line_start) + 1);

	if (isdigit((unsigned char)c)) return handle_num_literal(l);
	if (c == '\'') return handle_char_literal(l);
	if (c == '"') return handle_string_literal(l);

	if (isalpha((unsigned char)c) || c == '_') {
		int start_col = (int)(l->cursor - l->line_start) + 1;
		char *start = l->cursor++;
		while (isalnum((unsigned char)*l->cursor) || *l->cursor == '_') l->cursor++;
		size_t length = l->cursor - start;

		TokenType type = id_keyword(start, length);
		return make_token(type, start, length, l->line, start_col);
	}

	return handle_symbols(l);
}

// Debug shit
static const char *token_type_to_string (TokenType type)
{
	switch (type)
	{
	case TOK_NONE:	return "TOK_NONE";
	case TOK_INVALID:return "TOK_INVALID";
	case TOK_EOF:	return "TOK_EOF";
	case TOK_VAR:	return "TOK_VAR";
	case TOK_ID:	return "TOK_ID";
	case TOK_FN:	return "TOK_FN";
	case TOK_SEMCOL:return "TOK_SEMCOL";
	case TOK_COMMA:	return "TOK_COMMA";
	case TOK_COL:	return "TOK_COL";
	case TOK_LPAREN:return "TOK_LPAREN";
	case TOK_RPAREN:return "TOK_RPAREN";
	case TOK_LKEY:	return "TOK_LKEY";
	case TOK_RKEY:	return "TOK_RKEY";
	case TOK_LBRACE:return "TOK_LBRACE";
	case TOK_RBRACE:return "TOK_RBRACE";
	case TOK_LIT_i64: return "TOK_LIT_i64";
	case TOK_LIT_CHAR: return "TOK_LIT_CHAR";
	case TOK_LIT_STR: return "TOK_LIT_STR";
	case TOK_i64:	return "TOK_i64";
	case TOK_VOID:	return "TOK_VOID";
	case TOK_CHAR:	return "TOK_CHAR";
	case TOK_IF:	return "TOK_IF";
	case TOK_ELSE:	return "TOK_ELSE";
	case TOK_ELIF:	return "TOK_ELIF";
	case TOK_WHILE: return "TOK_WHILE";
	case TOK_FOR:	return "TOK_FOR";
	case TOK_RET:	return "TOK_RET";
	case TOK_ADD:	return "TOK_ADD";
	case TOK_SUB:	return "TOK_SUB";
	case TOK_STAR:	return "TOK_STAR";
	case TOK_SLASH: return "TOK_SLASH";
	case TOK_AND_A: return "TOK_AND_A";
	case TOK_OR_A:	return "TOK_OR_A";
	case TOK_XOR:	return "TOK_XOR";
	case TOK_RS:	return "TOK_RS";
	case TOK_LS:	return "TOK_LS";
	case TOK_NOT_A: return "TOK_NOT_A";
	case TOK_AND_L: return "TOK_AND_L";
	case TOK_OR_L:	return "TOK_OR_L";
	case TOK_NOT_L: return "TOK_NOT_L";
	case TOK_ASSIGN:return "TOK_ASSIGN";
	case TOK_EQ:	return "TOK_EQ";
	case TOK_NE:	return "TOK_NE";
	case TOK_GT:	return "TOK_GT";
	case TOK_LT:	return "TOK_LT";
	case TOK_GE:	return "TOK_GE";
	case TOK_LE:	return "TOK_LE";
	default:	return "UNKNOWN_TOKEN";
	}
}

static void print_escaped (const char *s, size_t len, char quote)
{
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		switch (c)
		{
		case '\\': printf("\\\\"); break;
		case '\n': printf("\\n"); break;
		case '\t': printf("\\t"); break;
		case '\r': printf("\\r"); break;
		case '\0': printf("\\0"); break;
		case '\'': printf(quote == '\'' ? "\\'" : "'"); break;
		case '"':  printf(quote == '"' ? "\\\"" : "\""); break;
		default: {
			if (isprint(c))
				putchar((char)c);
			else
				printf("\\x%02X", c);
			break;
		}
		}
	}
}

int dump_tokens (Lexer *l)
{
	Token t;
	do {
		t = get_token(l);
		const char *str_type = token_type_to_string(t.type);

		if (t.type == TOK_ID || t.type == TOK_LIT_STR) {
			printf("%s ", str_type);
			print_escaped(t.str, t.len, '"');
			printf("\n");
			continue;
		}
		if (t.type == TOK_LIT_CHAR) {
			printf("%s ", str_type);
			print_escaped(&t.chr, 1, '\'');
			printf("\n");
			continue;
		}

		if (t.type == TOK_LIT_i64) {
			printf("%s %ld\n", str_type, t.i64);
			continue;
		}
		printf("%s\n", str_type);
	} while (t.type != TOK_EOF);

	return l->err->err_count;
}
