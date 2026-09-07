#ifndef COMPILER_H
#define COMPILER_H

#include "arena/arena.h"
#include "lexer/lexer.h"
#include "error/error.h"

typedef struct Compiler {
	Arena arena;
	ErrorReporter err;
	char *src;
	Lexer lexer;
} Compiler;

void compiler_init (Compiler *comp);
int compiler_load_file (Compiler *comp, const char *path);
void compiler_destroy (Compiler *comp);

#endif
