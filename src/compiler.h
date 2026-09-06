#ifndef COMPILER_H
#define COMPILER_H

#include "lexer/lexer.h"
#include "arena/arena.h"

typedef struct Compiler {
	char *src;
	Lexer lexer;
} Compiler;

Compiler *init_compiler (char *src);

#endif
