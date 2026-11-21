#include "r2i/internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mangle/mangle.h"

#define R2I_PATH_MAX 4096

void r2i_printf (R2i *r, const char *fmt, ...)
{
	va_list ap;
	va_list probe;

	va_start(ap, fmt);
	va_copy(probe, ap);
	int need = vsnprintf(NULL, 0, fmt, probe);
	va_end(probe);
	if (need < 0) {
		va_end(ap);
		return;
	}
	if (r->len + (size_t)need + 1 > r->cap) {
		size_t cap = r->cap;

		while (r->len + (size_t)need + 1 > cap) cap <<= 1;
		char *code = realloc(r->code, cap);

		if (!code) {
			fprintf(stderr, "Not enough memory\n");
			exit(1);
		}
		r->code = code;
		r->cap = cap;
	}
	vsnprintf(r->code + r->len, (size_t)need + 1, fmt, ap);
	va_end(ap);
	r->len += (size_t)need;
}

void r2i_indent (R2i *r, int depth)
{
	for (int i = 0; i < depth; i++)
		r2i_printf(r, "\t");
}

int r2i_valid_name (const char *name, size_t len)
{
	if (!len) return 0;
	if ((len == 1 && name[0] == '.')
			|| (len == 2 && name[0] == '.' && name[1] == '.')) return 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)name[i];

		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
				|| (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return 0;
	}
	return 1;
}

static int module_file_name (Sema *sema, const char *source, const char **name, size_t *len)
{
	if (sema->mod_seen) {
		if (!r2i_valid_name(sema->mod_name, sema->mod_len)) {
			fprintf(stderr, "Error: module name '%.*s' may only contain letters, digits, '_' and '-'\n",
					(int)sema->mod_len, sema->mod_name);
			return 1;
		}
		*name = sema->mod_name;
		*len = sema->mod_len;
		return 0;
	}

	const char *base = strrchr(source, '/');
	size_t base_len = strlen(base ? base + 1 : source);
	base = base ? base + 1 : source;
	if (base_len <= 3) {
		fprintf(stderr, "Error: source file '%s' has no name before '.r2'\n", source);
		return 1;
	}
	*name = base;
	*len = base_len - 3;
	if (!r2i_valid_name(*name, *len)) {
		fprintf(stderr, "Error: source name '%s' may only contain letters, digits, '_' and '-' before '.r2'\n",
				base);
		return 1;
	}
	return 0;
}

int r2i_build_path (Sema *sema, const char *source, const char *dir,
		const char *ext, char **canon, char **path)
{
	char resolved[R2I_PATH_MAX];
	char rel[R2I_PATH_MAX];
	size_t dir_len = strlen(dir);

	const char *name;
	size_t len;
	if (module_file_name(sema, source, &name, &len)) return 1;

	if (!realpath(source, resolved)) {
		fprintf(stderr, "Error: cannot resolve the path of '%s'\n", source);
		return 1;
	}

	*canon = malloc(strlen(resolved) + 1);
	if (!*canon) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	strcpy(*canon, resolved);

	const char *subdir = "";
	size_t subdir_len = 0;
	if (!sema->mod_seen) {
		if (mangle_module_relative(source, rel, sizeof(rel))) return 1;
		const char *last = strrchr(rel, '/');

		if (last) {
			subdir = rel;
			subdir_len = (size_t)(last - rel) + 1;
		}
	}
	const char *slash = dir_len && dir[dir_len - 1] == '/' ? "" : "/";
	size_t ext_len = strlen(ext);
	size_t cap = dir_len + strlen(slash) + subdir_len + len + ext_len + 1;

	*path = malloc(cap);
	if (!*path) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	snprintf(*path, cap, "%s%s%.*s%.*s%s", dir, slash,
			(int)subdir_len, subdir, (int)len, name, ext);
	return 0;
}

int r2i_make_parent (const char *path)
{
	const char *slash = strrchr(path, '/');

	if (!slash || slash == path) return 0;
	size_t len = (size_t)(slash - path);
	char *dir = malloc(len + 1);

	if (!dir) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	memcpy(dir, path, len);
	dir[len] = '\0';
	int failed = r2i_make_dirs(dir);

	free(dir);
	return failed;
}

int r2i_make_dirs (const char *dir)
{
	char path[R2I_PATH_MAX];
	size_t len = strlen(dir);

	if (!len || len >= sizeof(path)) {
		fprintf(stderr, "Error: invalid modules directory '%s'\n", dir);
		return 1;
	}
	memcpy(path, dir, len + 1);
	for (size_t i = 1; i <= len; i++) {
		if (path[i] != '/' && path[i] != '\0') continue;
		char saved = path[i];

		path[i] = '\0';
		if (mkdir(path, 0755) != 0 && errno != EEXIST) {
			fprintf(stderr, "Error: could not create directory '%s': %s\n",
					path, strerror(errno));
			return 1;
		}
		path[i] = saved;
	}
	return 0;
}

static int same_file_bytes (const char *a, const char *b)
{
	FILE *fa = fopen(a, "rb");
	FILE *fb = fopen(b, "rb");
	char ba[4096];
	char bb[4096];
	int same = 1;

	if (!fa || !fb) {
		if (fa) fclose(fa);
		if (fb) fclose(fb);
		return 0;
	}
	while (1) {
		size_t na = fread(ba, 1, sizeof(ba), fa);
		size_t nb = fread(bb, 1, sizeof(bb), fb);

		if (na != nb || memcmp(ba, bb, na) != 0) {
			same = 0;
			break;
		}
		if (na < sizeof(ba)) break;
	}
	if (ferror(fa) || ferror(fb)) same = 0;
	fclose(fa);
	fclose(fb);
	return same;
}

int r2i_check_owner (const char *path, const char *canon)
{
	FILE *f = fopen(path, "rb");
	char line[R2I_PATH_MAX + 32];
	char *source = NULL;

	if (!f) {
		if (errno == ENOENT) return 0;
		fprintf(stderr, "Error: could not read '%s': %s\n", path, strerror(errno));
		return 1;
	}
	for (int i = 0; i < 16 && fgets(line, sizeof(line), f); i++) {
		if (strncmp(line, "// source: ", 11) != 0) {
			if (line[0] != '/') break;
			continue;
		}
		source = line + 11;
		break;
	}
	int failed = 0;

	if (!source) {
		fprintf(stderr, "Error: refusing to overwrite '%s': not generated by r2p\n", path);
		failed = 1;
	} else {
		char resolved[R2I_PATH_MAX];
		size_t n = strlen(source);

		while (n && (source[n - 1] == '\n' || source[n - 1] == '\r'))
			source[--n] = '\0';
		/* A different path with the same source (a moved checkout) or a path
		 * that no longer resolves (an old checkout) is not a different owner. */
		if (realpath(source, resolved) && strcmp(resolved, canon) != 0
				&& !same_file_bytes(source, canon)) {
			fprintf(stderr, "Error: refusing to overwrite '%s': it was generated from '%s'\n",
					path, source);
			failed = 1;
		}
	}
	fclose(f);
	return failed;
}

int r2i_write (R2i *r)
{
	if (!r->ready) return 0;
	if (r2i_make_dirs(r->dir) || r2i_make_parent(r->path)
			|| r2i_check_owner(r->path, r->canon)) return 1;

	size_t len = strlen(r->path);
	char *tmp = malloc(len + 5);

	if (!tmp) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	memcpy(tmp, r->path, len);
	memcpy(tmp + len, ".tmp", 5);
	FILE *f = fopen(tmp, "wb");
	if (!f) {
		fprintf(stderr, "Error: could not open '%s' for writing: %s\n",
				r->path, strerror(errno));
		free(tmp);
		return 1;
	}
	int failed = fwrite(r->code, 1, r->len, f) != r->len;

	if (fclose(f) != 0) failed = 1;
	if (!failed && rename(tmp, r->path) != 0) failed = 1;
	if (failed) {
		fprintf(stderr, "Error: could not write '%s'\n", r->path);
		remove(tmp);
	}
	free(tmp);
	return failed;
}

int r2i_has_init (IR *ir)
{
	IRFn *fn = ir->fns + ir->init_fn;

	for (uint32_t i = 0; i < fn->count; i++) {
		uint8_t op = ir->instrs[fn->start + i].op;

		if (op != IR_NOP && op != IR_RET) return 1;
	}
	return 0;
}

static void print_header_meta (R2i *r)
{
	Sema *s = r->sema;

	r2i_printf(r, "// depends:");
	for (uint16_t i = 0; i < s->dep_count; i++)
		r2i_printf(r, "%s%.*s", i ? ", " : " ",
				(int)s->dep_lens[i], s->deps[i]);
	r2i_printf(r, "\n// total_imports:");
	int first = 1;
	for (uint32_t i = 0; i < s->module_count; i++) {
		ImportModule *m = s->modules + i;

		r2i_printf(r, "%s%.*s", first ? " " : ", ",
				(int)m->name_len, m->name);
		first = 0;
	}
	r2i_printf(r, "\n\n");
}

int r2i_generate (R2i *r, IR *ir, Sema *sema, const char *source, const char *dir)
{
	memset(r, 0, sizeof(*r));
	r->ir = ir;
	r->sema = sema;
	r->source = source;
	r->dir = dir;

	if (r2i_build_path(sema, source, dir, ".r2i", &r->canon, &r->path)) return 1;
	r2i_build_types(r);
	r2i_collect_imports(r);
	r->cap = 1024;
	r->code = malloc(r->cap);
	if (!r->code) {
		fprintf(stderr, "Not enough memory\n");
		exit(1);
	}
	r2i_printf(r, "// r2i v1\n// source: %s\n// target: %s\n// module: %s\n"
			"// has_dynamic_init: %d\n// imports:",
			r->canon, ir->target, ir->mod, r2i_has_init(ir));
	int first = 1;
	for (uint32_t i = 0; i < sema->module_count; i++) {
		ImportModule *m = sema->modules + i;

		if (!r->import_used || !r->import_used[i]) continue;
		r2i_printf(r, "%s%.*s", first ? " " : ", ", (int)m->name_len, m->name);
		first = 0;
	}
	r2i_printf(r, "\n");
	print_header_meta(r);
	r2i_make_decls(r);
	r2i_make_init(r);
	r->ready = 1;
	return r->failed;
}

void r2i_free (R2i *r)
{
	free(r->code);
	free(r->canon);
	free(r->path);
	memset(r, 0, sizeof(*r));
}
