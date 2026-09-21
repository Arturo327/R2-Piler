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
	arena_init(&comp->sym_arena);
	arena_init(&comp->ir_arena);
	arena_init(&comp->gen_arena);
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

static int get_file (Compiler *comp, const char *file)
{
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
	return 0;
}

static int check_source_limits (const char *file, const char *src)
{
	size_t lines;
	size_t longest;

	error_source_stats(src, &lines, &longest);
	if (lines > UINT16_MAX) {
		fprintf(stderr, "Error: %s has more than %u lines\nPlease, for your own good and your co-workers, I strongly recomend you to divide this enormous file\n",
				file, (unsigned)UINT16_MAX);
		return 1;
	}
	if (longest >= UINT16_MAX) {
		fprintf(stderr, "Error: %s has a line longer than %u characters. I don't know if you use an IMAX screen to code, but I think your co-workers don't\n",
				file, (unsigned)(UINT16_MAX - 1));
		return 1;
	}
	return 0;
}

static int compiler_load_file (Compiler *comp, const char *file)
{
	if (get_file(comp, file)) return 1;
	if (check_source_limits(file, comp->src)) return 1;

	error_init(&comp->err, file, comp->src, &comp->arena);
	init_lexer(&comp->lexer, comp->src, &comp->arena, &comp->err);
	init_parser(&comp->parser, &comp->lexer, &comp->ast_arena, &comp->err);
	sema_init(&comp->sema, &comp->sym_arena, &comp->parser.ast, &comp->err);
	return 0;
}

static void release_frontend (Compiler *c)
{
	arena_destroy(&c->ast_arena);
	arena_destroy(&c->sym_arena);

	memset(&c->parser.ast, 0, sizeof(c->parser.ast));
	memset(&c->sema.table, 0, sizeof(c->sema.table));
	c->sema.ast = NULL;
	c->sema.init_order = NULL;
	c->sema.init_order_count = 0;

	c->ir.ast = NULL;
	c->ir.symtab = NULL;
	c->ir.init_order = NULL;
	c->ir.init_order_count = 0;
}

int compile (Compiler *c, CompilerOpts *opts)
{
	int dumping = opts->dump_tokens || opts->dump_ast
			|| opts->dump_symbols || opts->dump_ir;

	if (compiler_load_file(c, opts->path)) return 1;
	if (!dumping && init_codegen(&c->codegen, opts->arch, &c->gen_arena,
			&c->ir, opts->out))
		return 1;
	if (opts->dump_tokens)
		return dump_tokens(&c->lexer) ? 1 : 0;

	parse(&c->parser);
	if (opts->dump_ast) {
		dump_ast(&c->parser.ast);
		return c->err.err_count ? 1 : 0;
	}
	if (c->err.err_count) return 1;

	sema_run(&c->sema);
	if (opts->dump_symbols) {
		dump_symbols(&c->sema.table);
		return c->err.err_count ? 1 : 0;
	}
	if (c->err.err_count) return 1;

	ir_init(&c->ir, &c->ir_arena, &c->sema);
	ir_gen(&c->ir);
	if (opts->dump_ir) {
		dump_ir(&c->ir);
		return 0;
	}
	release_frontend(c);
	return codegen_run(&c->codegen);
}

void compiler_destroy (Compiler *comp)
{
	arena_destroy(&comp->arena);
	arena_destroy(&comp->ast_arena);
	arena_destroy(&comp->sym_arena);
	arena_destroy(&comp->ir_arena);
	arena_destroy(&comp->gen_arena);
	memset(comp, 0, sizeof(*comp));
}
