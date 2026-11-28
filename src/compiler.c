#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "compiler.h"

void compiler_init (Compiler *comp)
{
	memset(comp, 0, sizeof(*comp));
	init_types();
	arena_init(&comp->arena);
	arena_init(&comp->ast_arena);
	arena_init(&comp->sym_arena);
	arena_init(&comp->ir_arena);
	arena_init(&comp->gen_arena);
	arena_init(&comp->opt_arena);
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
		fprintf(stderr, "Error: %s has more than %u lines. Please, for your own good and your co-workers, I strongly recomend you to divide this enormous file\n",
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
	c->ir.statics = NULL;
	c->ir.static_count = 0;
}

static int werror_enabled;

static inline void print_err_count (Compiler *c)
{
	if (werror_enabled && c->err.warn_count) {
		c->err.err_count += c->err.warn_count;
		c->err.warn_count = 0;
	}
	if (!c->err.err_count && !c->err.warn_count) return;
	fprintf(stderr, "%d error%s, %d warning%s\n",
			c->err.err_count, c->err.err_count == 1 ? "" : "s",
			c->err.warn_count, c->err.warn_count == 1 ? "" : "s");
}

static int compile_setup (Compiler *c, CompilerOpts *opts, int dumping)
{
	if (compiler_load_file(c, opts->path)) return 1;
	if (!dumping && mangle_module(&c->arena, opts->path, &c->mod)) return 1;
	if (!dumping) {
		c->codegen.arena = &c->gen_arena;
		if (init_codegen(&c->codegen, opts->arch, &c->ir, opts->out, opts->opt_level))
			return 1;
	}
	return 0;
}

static int compile_parse (Compiler *c, CompilerOpts *opts)
{
	parse(&c->parser);

	if (opts->dump_ast) {
		dump_ast(&c->parser.ast);
		print_err_count(c);
		return c->err.err_count ? 1 : 0;
	}
	if (!c->err.err_count) return -1;
	print_err_count(c);
	return 1;
}

static int compile_sema (Compiler *c, CompilerOpts *opts)
{
	if (imports_load(c, opts)) {
		print_err_count(c);
		return 1;
	}
	sema_run(&c->sema);

	if (opts->dump_symbols) {
		dump_symbols(&c->sema);
		print_err_count(c);
		return c->err.err_count ? 1 : 0;
	}
	if (!c->err.err_count) return -1;
	print_err_count(c);
	return 1;
}

static int apply_module_name (Compiler *c)
{
	if (!c->sema.mod_seen || !c->sema.mod_len) return 0;
	if (mangle_module_name(&c->arena, c->sema.mod_name,
			c->sema.mod_len, &c->mod)) {
		fprintf(stderr, "Error: invalid module name\n");
		return 1;
	}
	return 0;
}

static int compile_ir (Compiler *c, CompilerOpts *opts)
{
	if (apply_module_name(c)) return 1;

	ir_init(&c->ir, &c->ir_arena, &c->sema,
			opts->opt_level == OPT_FULL && !opts->dump_ir, c->mod);
	c->ir.target = c->codegen.target;
	ir_gen(&c->ir);

	if (!opts->dump_ir) return -1;

	dump_ir(&c->ir);
	print_err_count(c);
	return 0;
}

static int compile_optimize (Compiler *c, CompilerOpts *opts)
{
	if (opts->opt_level >= OPT_BASIC)
		optimize_ir(&c->ir, &c->opt_arena, opts->opt_level);

	if (!opts->dump_opt) return -1;

	dump_ir(&c->ir);
	print_err_count(c);
	return 0;
}

static int compile_interfaces (Compiler *c, CompilerOpts *opts, R2i *r2i, CHeader *header)
{
	if (opts->r2i && r2i_generate(r2i, &c->ir, &c->sema, opts->path,
			opts->r2i_dir)) {
		r2i_free(r2i);
		print_err_count(c);
		return 1;
	}
	if (opts->header && header_generate(header, &c->ir, &c->sema,
			opts->path, opts->header_dir)) {
		r2i_free(r2i);
		header_free(header);
		print_err_count(c);
		return 1;
	}
	return -1;
}

static int compile_backend (Compiler *c, R2i *r2i, CHeader *header)
{
	print_err_count(c);
	int status = codegen_run(&c->codegen);

	if (!status && r2i->ready) status = r2i_write(r2i);
	if (!status && header->ready) status = header_write(header);
	r2i_free(r2i);
	header_free(header);
	return status;
}

int compile (Compiler *c, CompilerOpts *opts)
{
	int dumping = opts->dump_tokens || opts->dump_ast
			|| opts->dump_symbols || opts->dump_ir || opts->dump_opt;

	werror_enabled = opts->werror;
	if (compile_setup(c, opts, dumping)) return 1;
	if (opts->dump_tokens) return dump_tokens(&c->lexer) ? 1 : 0;

	int stage = compile_parse(c, opts);
	if (stage != -1) return stage;
	stage = compile_sema(c, opts);
	if (stage != -1) return stage;

	R2i r2i;
	CHeader header;

	memset(&r2i, 0, sizeof(r2i));
	memset(&header, 0, sizeof(header));
	stage = compile_ir(c, opts);
	if (stage != -1) return stage;
	stage = compile_optimize(c, opts);
	if (stage != -1) return stage;
	stage = compile_interfaces(c, opts, &r2i, &header);
	if (stage != -1) return stage;

	release_frontend(c);
	int status = compile_backend(c, &r2i, &header);

	if (werror_enabled && (c->err.err_count || c->err.warn_count)) return 1;
	return status;
}

void compiler_destroy (Compiler *comp)
{
	types_destroy();
	arena_destroy(&comp->arena);
	arena_destroy(&comp->ast_arena);
	arena_destroy(&comp->sym_arena);
	arena_destroy(&comp->ir_arena);
	arena_destroy(&comp->gen_arena);
	arena_destroy(&comp->opt_arena);
	free(comp->codegen.code);
	memset(comp, 0, sizeof(*comp));
}
