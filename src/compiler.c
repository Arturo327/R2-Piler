#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

Compiler *init_compiler (char *src)
{
	Compiler *comp = arena_alloc(sizeof(Compiler));
	memset(comp, 0, sizeof(Compiler));

	comp->src = src;
	init_lexer(&comp->lexer, src);
	
	return comp;
}
