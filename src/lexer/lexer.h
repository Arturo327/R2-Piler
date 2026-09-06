#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>

typedef enum {
	TOK_NONE = 0, TOK_INVALID, TOK_EOF,
	TOK_VAR, TOK_ID, TOK_FN, TOK_SEMCOL, TOK_COMMA, TOK_COL,		// Punctuation
	TOK_LPAREN, TOK_RPAREN, TOK_LKEY, TOK_RKEY, TOK_LBRACE, TOK_RBRACE,	// Parentheses, Braces
	TOK_LIT_i64, TOK_LIT_CHAR, TOK_LIT_STR,					// Literals
	TOK_i64, TOK_CHAR,							// Types
	TOK_IF, TOK_ELSE, TOK_ELIF, TOK_WHILE, TOK_FOR, TOK_RET,		// Control, Branching
	TOK_ADD, TOK_SUB, TOK_STAR, TOK_SLASH,					// Operations
	TOK_AND_A, TOK_OR_A, TOK_XOR, TOK_RS, TOK_LS, TOK_NOT_A,		// Bitwise operations
	TOK_AND_L, TOK_OR_L, TOK_NOT_L,						// Logic operations
	TOK_ASSIGN, TOK_EQ, TOK_NE, TOK_GT, TOK_LT, TOK_GE, TOK_LE		// Logic operations
} TokenType;

typedef struct Token {
	TokenType type;
	int line;
	union {
		struct {
			char *string;
			size_t length;
		};
		int64_t i64;
		char chr;
	};
} Token;

typedef struct Lexer {
	char *cursor;
	char *line_start;
	int line;
	int err_count;
} Lexer;

void init_lexer (Lexer *lexer, char *src);
int dump_tokens (Lexer *l);
Token get_token (Lexer *l);

#endif
