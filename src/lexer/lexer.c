#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

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

static int digit_value (char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int numeric_prefix_base (Lexer *l)
{
	if (l->cursor[0] != '0')
		return 10;

	char c = l->cursor[1];
	if (c == 'x' || c == 'X') {
		l->cursor += 2;
		return 16;
	}
	if (c == 'o' || c == 'O') {
		l->cursor += 2;
		return 8;
	}
	if (c == 'b' || c == 'B') {
		l->cursor += 2;
		return 2;
	}
	return 10;
}

static int scan_digits (Lexer *l, int base, uint64_t *value, int *overflow)
{
	int consumed = 0;

	while (1) {
		int digit = digit_value(*l->cursor);
		if (digit < 0 || digit >= base) break;

		if (*value > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base)
			*overflow = 1;
		*value = *value * (uint64_t)base + (uint64_t)digit;

		l->cursor++;
		consumed = 1;
	}

	return consumed;
}

static int consume_unsigned_suffix (Lexer *l)
{
	if (*l->cursor == 'u' || *l->cursor == 'U') {
		l->cursor++;
		return 1;
	}
	return 0;
}

static int check_invalid_trailing (Lexer *l, char *start, int start_line, int start_col)
{
	if (!isalnum((unsigned char)*l->cursor) && *l->cursor != '_')
		return 0;

	while (isalnum((unsigned char)*l->cursor) || *l->cursor == '_')
		l->cursor++;

	error_report(l->err, ERR_ERROR, loc_at(start_line, start_col,
			(int)(l->cursor - start)), "invalid digit in numeric literal");
	return 1;
}

static Token handle_num_literal (Lexer *l)
{
	char *start = l->cursor;
	int start_line = l->line;
	int start_col = (int)(start - l->line_start) + 1;
	int base = numeric_prefix_base(l);

	uint64_t value = 0;
	int overflow = 0;
	int had_digits = scan_digits(l, base, &value, &overflow);

	if (base != 10 && !had_digits) {
		error_report(l->err, ERR_ERROR, loc_at(start_line, start_col,
				(int)(l->cursor - start)), "expected digits after numeric literal prefix");
		return make_token(TOK_INVALID, NULL, (uint16_t)(l->cursor - start), start_line, start_col);
	}

	uint8_t is_unsigned = consume_unsigned_suffix(l);

	if (check_invalid_trailing(l, start, start_line, start_col))
		return make_token(TOK_INVALID, NULL, (uint16_t)(l->cursor - start), start_line, start_col);

	if (overflow) error_report(l->err, ERR_WARNING, loc_at(start_line, start_col,
			(int)(l->cursor - start)),
			"literal does not fit in 64 bits, truncated to %llu",
			(unsigned long long)value);

	Token token = {
		.type = is_unsigned ? TOK_LIT_u64 : TOK_LIT_i64,
		.line = start_line,
		.u64 = value,
		.len = (uint16_t)(l->cursor - start),
		.col = start_col,
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

static void track_continuation_lines (Lexer *l, char *from, char *to)
{
	for (char *c = from; c < to; c++) {
		if (c[0] != '\\')
			continue;
		if (c[1] == '\n') {
			l->line++;
			l->line_start = c + 2;
		}
		c++;
	}
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
		if (!decode_escape(l->err, escaped, &decoded, loc)) {
			track_continuation_lines(l, l->cursor, end);
			return make_token(TOK_INVALID, NULL, 0, start_line, start_col);
		}
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
		track_continuation_lines(l, l->cursor, end);
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
	if (c == '\0') {
		error_report(l->err, ERR_ERROR, loc_at(l->line, (int)(l->cursor - l->line_start), 1),
				"char literal is not closed");
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
	{"as", TOK_AS},
	{"fn", TOK_FN},
	{"i8", TOK_i8},
	{"if", TOK_IF},
	{"u8", TOK_u8},
	{"for", TOK_FOR},
	{"i16", TOK_i16},
	{"i32", TOK_i32},
	{"i64", TOK_i64},
	{"u16", TOK_u16},
	{"u32", TOK_u32},
	{"u64", TOK_u64},
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

typedef struct TwoCharOp {
	char first;
	char second;
	TokenType type;
} TwoCharOp;

static const TwoCharOp two_char_ops[] = {
	{'=', '=', TOK_EQ}, {'!', '=', TOK_NE},
	{'>', '>', TOK_RS}, {'>', '=', TOK_GE},
	{'<', '<', TOK_LS}, {'<', '=', TOK_LE},
	{'&', '&', TOK_AND_L}, {'|', '|', TOK_OR_L}
};

static const uint8_t one_char_ops[256] = {
	['+'] = TOK_ADD, ['-'] = TOK_SUB, ['*'] = TOK_STAR,
	['/'] = TOK_SLASH, ['%'] = TOK_PERCENT, ['^'] = TOK_XOR,
	[';'] = TOK_SEMCOL, [':'] = TOK_COL, [','] = TOK_COMMA,
	['~'] = TOK_NOT_A, ['('] = TOK_LPAREN, [')'] = TOK_RPAREN,
	['['] = TOK_LBRACE, [']'] = TOK_RBRACE,
	['{'] = TOK_LKEY, ['}'] = TOK_RKEY,
	['='] = TOK_ASSIGN, ['!'] = TOK_NOT_L,
	['>'] = TOK_GT, ['<'] = TOK_LT,
	['&'] = TOK_AND_A, ['|'] = TOK_OR_A
};

static Token handle_symbols (Lexer *l)
{
	int col = (int)(l->cursor - l->line_start) + 1;
	char c = *l->cursor++;
	size_t count = sizeof(two_char_ops) / sizeof(two_char_ops[0]);

	for (size_t i = 0; i < count; i++) {
		if (two_char_ops[i].first != c || two_char_ops[i].second != *l->cursor)
			continue;
		l->cursor++;
		return make_token(two_char_ops[i].type, NULL, 2, l->line, col);
	}
	if (one_char_ops[(unsigned char)c] != TOK_NONE)
		return make_token(one_char_ops[(unsigned char)c], NULL, 1, l->line, col);

	if (isprint((unsigned char)c))
		error_report(l->err, ERR_ERROR, loc_at(l->line, col, 1),
				"character '%c' is not valid", c);
	else error_report(l->err, ERR_ERROR, loc_at(l->line, col, 1),
			"character '\\x%02X' is not valid", (unsigned char)c);
	return make_token(TOK_INVALID, NULL, 0, l->line, col);
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

		if (length > UINT16_MAX) {
			error_report(l->err, ERR_ERROR, loc_at(l->line, start_col, 1),
					"identifier is too long");
			return make_token(TOK_INVALID, NULL, 0, l->line, start_col);
		}

		TokenType type = id_keyword(start, length);
		return make_token(type, start, length, l->line, start_col);
	}

	return handle_symbols(l);
}

// Debug shit

#define TN(t) [t] = #t
static const char *const token_names[TOK_COUNT] = {
	TN(TOK_NONE), TN(TOK_INVALID), TN(TOK_EOF), TN(TOK_VAR), TN(TOK_ID),
	TN(TOK_FN), TN(TOK_SEMCOL), TN(TOK_COMMA), TN(TOK_COL), TN(TOK_LPAREN),
	TN(TOK_RPAREN), TN(TOK_LKEY), TN(TOK_RKEY), TN(TOK_LBRACE), TN(TOK_RBRACE),
	TN(TOK_LIT_i64), TN(TOK_LIT_u64), TN(TOK_LIT_CHAR), TN(TOK_LIT_STR),
	TN(TOK_i8), TN(TOK_u8), TN(TOK_i16), TN(TOK_u16), TN(TOK_i32), TN(TOK_u32),
	TN(TOK_i64), TN(TOK_u64), TN(TOK_CHAR), TN(TOK_VOID), TN(TOK_IF),
	TN(TOK_ELSE), TN(TOK_ELIF), TN(TOK_WHILE), TN(TOK_FOR), TN(TOK_RET), TN(TOK_AS),
	TN(TOK_ADD), TN(TOK_SUB), TN(TOK_STAR), TN(TOK_SLASH), TN(TOK_PERCENT),
	TN(TOK_AND_A), TN(TOK_OR_A), TN(TOK_XOR), TN(TOK_RS), TN(TOK_LS),
	TN(TOK_NOT_A), TN(TOK_AND_L), TN(TOK_OR_L), TN(TOK_NOT_L),
	TN(TOK_ASSIGN), TN(TOK_EQ), TN(TOK_NE), TN(TOK_GT), TN(TOK_LT),
	TN(TOK_GE), TN(TOK_LE)
};

static const char *token_type_to_string (TokenType type)
{
	if (type >= TOK_COUNT || !token_names[type])
		return "UNKNOWN_TOKEN";
	return token_names[type];
}

void print_escaped (const char *s, size_t len, char quote)
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
		} else if (t.type == TOK_LIT_CHAR) {
			printf("%s ", str_type);
			print_escaped(&t.chr, 1, '\'');
			printf("\n");
		} else if (t.type == TOK_LIT_i64 || t.type == TOK_LIT_u64) {
			printf("%s %llu\n", str_type, (unsigned long long)t.u64);
		} else printf("%s\n", str_type);
	} while (t.type != TOK_EOF);

	return l->err->err_count;
}
