#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "compiler.h"

void compiler_init (Compiler *comp)
{
	memset(comp, 0, sizeof(*comp));
	arena_init(&comp->arena);
	arena_init(&comp->ast_arena);
}

static long file_size (FILE *f, const char *file)
{
	if (fseek(f, 0, SEEK_END) != 0) {
		fprintf(stderr, "Error: cannot seek in %s\n", file);
		return -1;
	}
	long size = ftell(f);
	if (size < 0) {
		fprintf(stderr, "Error: cannot determine size of %s\n", file);
		return -1;
	}
	rewind(f);
	return size;
}

int compiler_load_file (Compiler *comp, const char *file)
{
	error_init(&comp->err, file);

	struct stat st;
	if (stat(file, &st) != 0 || !S_ISREG(st.st_mode)) {
		fprintf(stderr, "Error: %s is not a regular file\n", file);
		return 1;
	}

	FILE *f = fopen(file, "rb");
	if (!f) {
		fprintf(stderr, "Error: could not read file %s\n", file);
		return 1;
	}
	long sz = file_size(f,file);
	if (sz < 0) {
		fclose(f);
		return 1;
	}
	size_t src_size = (size_t)sz;

	comp->src = arena_alloc(&comp->arena, src_size + 1);
	if (fread(comp->src, sizeof(char), src_size, f) != src_size) {
		fprintf(stderr, "Could not read the file %s correctly\n", file);
		fclose(f);
		return 1;
	}

	comp->src[src_size] = '\0';
	fclose(f);

	if (memchr(comp->src, '\0', src_size) != NULL) {
		fprintf(stderr, "Error: %s contains embedded NUL bytes\n", file);
		return 1;
	}

	init_lexer(&comp->lexer, comp->src, &comp->arena, &comp->err);
	init_parser(&comp->parser, &comp->lexer, &comp->ast_arena, &comp->err);

	return 0;
}

void compiler_destroy (Compiler *comp)
{
	arena_destroy(&comp->arena);
	arena_destroy(&comp->ast_arena);
	memset(comp, 0, sizeof(*comp));
}
