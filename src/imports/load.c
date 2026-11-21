#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "imports/internal.h"
#include "sema/common.h"

typedef struct TextLine {
	const char *str;
	size_t len;
} TextLine;

typedef struct HeaderScan {
	int version;
	int module;
	int init;
	int imports;
} HeaderScan;

typedef struct ImportRequest {
	const char *name;
	uint16_t len;
	const char *alias;
	uint16_t alias_len;
	const char *path;
	uint32_t use;
	int direct;
} ImportRequest;

typedef struct FolderEntry {
	const char *name;
	uint8_t is_dir;
} FolderEntry;

typedef struct ModName {
	const char *str;
	uint16_t len;
} ModName;

typedef struct FolderLoad {
	ModName canon;
	ModName alias;
	uint32_t use;
	int depth;
} FolderLoad;

#define FOLDER_MAX_DEPTH 32

static uint32_t load_module (ImportLoader *l, const ImportRequest *req);
static uint32_t load_path (ImportLoader *l, const ImportRequest *req);
static void load_folder (ImportLoader *l, const char *path, const FolderLoad *fl);

uint32_t import_find_module (Sema *s, const char *name, uint16_t len)
{
	for (uint32_t i = 0; i < s->module_count; i++) {
		ImportModule *m = s->modules + i;

		if (m->name_len == len && memcmp(m->name, name, len) == 0)
			return i;
	}
	return NO_MODULE;
}

ImportedDecl *import_find_item (ImportModule *m, const char *name, uint16_t len)
{
	for (uint16_t i = 0; i < m->item_count; i++) {
		ImportedDecl *item = m->items + i;

		if (item->len == len && memcmp(item->name, name, len) == 0)
			return item;
	}
	return NULL;
}

static inline ErrorLoc use_loc (ImportLoader *l, uint32_t node)
{
	return node_loc(l->c->parser.ast.nodes + node);
}

static char *source_dir (Arena *a, const char *path)
{
	const char *slash = strrchr(path, '/');
	if (!slash) return ".";
	size_t len = (size_t)(slash - path);
	if (!len) return "/";
	char *dir = arena_alloc(a, len + 1);
	memcpy(dir, path, len);
	dir[len] = '\0';
	return dir;
}

static const char **search_dirs (ImportLoader *l, uint32_t *count)
{
	Arena *a = &l->c->arena;
	const CompilerOpts *o = l->opts;
	uint32_t total = 2u + (uint32_t)o->import_path_count;
	const char **dirs = arena_alloc(a, sizeof(char *) * total);
	uint32_t n = 0;

	dirs[n++] = source_dir(a, o->path);
	dirs[n++] = o->r2i_dir ? o->r2i_dir : "modules";
	for (int i = 0; i < o->import_path_count; i++) dirs[n++] = o->import_paths[i];
	*count = n;
	return dirs;
}

static char *module_fs_path (ImportLoader *l, const char *name, uint16_t len, uint16_t *out_len)
{
	char *out = arena_alloc(&l->c->arena, (size_t)len + 1);
	uint16_t n = 0;

	for (uint16_t i = 0; i < len; i++) {
		if (name[i] == ':' && i + 1 < len && name[i + 1] == ':') {
			out[n++] = '/';
			i++;
		} else out[n++] = name[i];
	}
	out[n] = '\0';
	*out_len = n;
	return out;
}

static char *make_path (ImportLoader *l, const char *dir, const char *rel, uint16_t rel_len, const char *suffix)
{
	size_t dlen = strlen(dir);
	int slash = dlen && dir[dlen - 1] == '/';
	size_t slen = strlen(suffix);
	size_t total = dlen + (slash ? 0 : 1) + rel_len + slen;
	char *out = arena_alloc(&l->c->arena, total + 1);

	snprintf(out, total + 1, "%s%s%.*s%s", dir, slash ? "" : "/", (int)rel_len, rel, suffix);
	return out;
}

static int dir_has_r2i (ImportLoader *l, const char *dir, int depth)
{
	if (depth > FOLDER_MAX_DEPTH) return 0;
	DIR *d = opendir(dir);

	if (!d) return 0;
	int found = 0;
	struct dirent *de;

	while ((de = readdir(d)) != NULL && !found) {
		const char *name = de->d_name;
		uint16_t len = (uint16_t)strlen(name);
		struct stat st;
		if (!len || name[0] == '.') continue;
		char *full = make_path(l, dir, name, len, "");
		if (stat(full, &st) != 0) continue;
		if (S_ISDIR(st.st_mode)) found = dir_has_r2i(l, full, depth + 1);
		else if (S_ISREG(st.st_mode) && len > 4 && !strcmp(name + len - 4, ".r2i"))
			found = 1;
	}
	closedir(d);
	return found;
}

typedef struct PathFind {
	const char *suffix;
	uint8_t want_dir;
	uint8_t need_r2i;
} PathFind;

static const PathFind FIND_FILE = { ".r2i", 0, 0 };
static const PathFind FIND_DIR = { "", 1, 1 };
static const PathFind FIND_ANY_DIR = { "", 1, 0 };

static const char *find_in_dirs (ImportLoader *l, const char *name, uint16_t len, const PathFind *how)
{
	uint16_t fs_len;
	char *fs = module_fs_path(l, name, len, &fs_len);
	uint32_t count;
	const char **dirs = search_dirs(l, &count);

	for (uint32_t i = 0; i < count; i++) {
		struct stat st;
		const char *path = make_path(l, dirs[i], fs, fs_len, how->suffix);
		int found = 0;

		if (stat(path, &st) != 0) continue;
		if (how->want_dir)
			found = S_ISDIR(st.st_mode) && (!how->need_r2i || dir_has_r2i(l, path, 0));
		else found = S_ISREG(st.st_mode);
		if (found) return path;
	}
	return NULL;
}

static char *read_bytes (ImportLoader *l, const char *path, long *size)
{
	struct stat st;

	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
		fprintf(stderr, "Error: '%s' is not a regular file\n", path);
		return NULL;
	}
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Error: could not read '%s'\n", path);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) != 0 || (*size = ftell(f)) < 0) {
		fprintf(stderr, "Error: cannot determine the size of '%s'\n", path);
		fclose(f);
		return NULL;
	}
	rewind(f);
	char *src = arena_alloc(&l->c->arena, (size_t)*size + 1);
	if (fread(src, 1, (size_t)*size, f) != (size_t)*size) {
		fprintf(stderr, "Error: could not read '%s'\n", path);
		fclose(f);
		return NULL;
	}
	fclose(f);
	src[*size] = '\0';
	return src;
}

static char *read_module_file (ImportLoader *l, const char *path)
{
	long size;
	char *src = read_bytes(l, path, &size);

	if (!src) return NULL;
	if (memchr(src, '\0', (size_t)size)) {
		fprintf(stderr, "Error: '%s' contains embedded NUL bytes\n", path);
		return NULL;
	}

	size_t lines;
	size_t longest;
	error_source_stats(src, &lines, &longest);
	if (lines > UINT16_MAX || longest >= UINT16_MAX) {
		fprintf(stderr, "Error: '%s' is too large to import\n", path);
		return NULL;
	}
	return src;
}

static int next_line (char **cursor, TextLine *line)
{
	char *start = *cursor;

	if (!*start) return 0;
	char *nl = strchr(start, '\n');
	line->str = start;
	line->len = nl ? (size_t)(nl - start) : strlen(start);
	*cursor = nl ? nl + 1 : start + line->len;
	return 1;
}

static void trim_line (TextLine *line)
{
	while (line->len && (line->str[line->len - 1] == ' '
			|| line->str[line->len - 1] == '\t'
			|| line->str[line->len - 1] == '\r'))
		line->len--;
	while (line->len && (*line->str == ' ' || *line->str == '\t')) {
		line->str++;
		line->len--;
	}
}

static int line_value (const TextLine *line, const char *key, const char **val, size_t *len)
{
	size_t klen = strlen(key);

	if (line->len < klen || memcmp(line->str, key, klen) != 0) return 0;
	const char *v = line->str + klen;
	size_t n = line->len - klen;
	while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r')) n--;
	while (n && (*v == ' ' || *v == '\t')) {
		v++;
		n--;
	}
	*val = v;
	*len = n;
	return 1;
}

static int header_error (const char *path, const char *msg)
{
	fprintf(stderr, "Error: invalid .r2i file '%s': %s\n", path, msg);
	return 1;
}

static int valid_module_name (const char *s, size_t len)
{
	size_t seg = 0;

	if (!len) return 0;
	for (size_t i = 0; i < len;) {
		if (s[i] == ':') {
			if (i + 1 >= len || s[i + 1] != ':' || !r2i_valid_name(s + seg, i - seg))
				return 0;
			i += 2;
			seg = i;
			continue;
		}
		i++;
	}
	return r2i_valid_name(s + seg, len - seg);
}

static int parse_deps (ImportLoader *l, const char *path, const char *val, size_t len, R2iHeader *h)
{
	size_t seg = 0;

	if (!len) return 0;
	uint16_t count = 1;
	for (size_t i = 0; i < len; i++)
		if (val[i] == ',') count++;
	h->deps = arena_alloc(&l->c->arena, sizeof(char *) * count);
	h->dep_lens = arena_alloc(&l->c->arena, sizeof(uint16_t) * count);

	while (seg <= len) {
		size_t end = seg;
		while (end < len && val[end] != ',') end++;
		size_t a = seg;
		size_t b = end;

		while (a < b && (val[a] == ' ' || val[a] == '\t')) a++;
		while (b > a && (val[b - 1] == ' ' || val[b - 1] == '\t')) b--;
		if (!valid_module_name(val + a, b - a))
			return header_error(path, "invalid module name in '// imports:'");
		h->deps[h->dep_count] = val + a;
		h->dep_lens[h->dep_count] = (uint16_t)(b - a);
		h->dep_count++;
		if (end == len) break;
		seg = end + 1;
	}
	return 0;
}

static int header_field (ImportLoader *l, const char *path, TextLine *line, R2iHeader *h, HeaderScan *scan)
{
	const char *val;
	size_t len;

	if (line_value(line, "// source:", &val, &len)) {
		h->source = val;
		h->source_len = len;
		return 0;
	}
	if (line_value(line, "// module:", &val, &len)) {
		if (!len) return header_error(path, "empty module id");
		if (len > UINT16_MAX) return header_error(path, "module id is too long");
		h->mod = val;
		h->mod_len = len;
		scan->module = 1;
		return 0;
	}
	if (line_value(line, "// has_dynamic_init:", &val, &len)) {
		if (len != 1 || (val[0] != '0' && val[0] != '1'))
			return header_error(path, "invalid 'has_dynamic_init' value");
		h->has_init = (uint16_t)(val[0] - '0');
		scan->init = 1;
		return 0;
	}
	if (line_value(line, "// imports:", &val, &len)) {
		scan->imports = 1;
		return parse_deps(l, path, val, len, h);
	}
	return 0;
}

static int parse_header (ImportLoader *l, const char *path, char *src, R2iHeader *h)
{
	char *cursor = src;
	TextLine line;
	HeaderScan scan = {0};
	int line_no = 0;

	memset(h, 0, sizeof(*h));
	while (next_line(&cursor, &line)) {
		line_no++;
		trim_line(&line);
		if (line_no == 1 && line.len == 9 && memcmp(line.str, "// r2i v1", 9) == 0) {
			scan.version = 1;
			continue;
		}
		if (header_field(l, path, &line, h, &scan)) return 1;
	}
	if (!scan.version) return header_error(path, "missing '// r2i v1' header");
	if (!scan.module) return header_error(path, "missing '// module:' header");
	if (!scan.init) return header_error(path, "missing '// has_dynamic_init:' header");
	return 0;
}

static int same_source (const char *a, const char *b)
{
	char ra[PATH_MAX];
	char rb[PATH_MAX];

	if (!realpath(a, ra) || !realpath(b, rb)) return 0;
	return strcmp(ra, rb) == 0;
}

static int duplicate_module (Sema *s, const R2iHeader *h)
{
	for (uint32_t i = 0; i < s->module_count; i++) {
		ImportModule *m = s->modules + i;

		if (m->mod_len == h->mod_len
				&& memcmp(m->mod, h->mod, h->mod_len) == 0)
			return 1;
	}
	return 0;
}

static int check_module (ImportLoader *l, const R2iHeader *h, uint32_t use)
{
	char source[PATH_MAX];

	if (h->source && h->source_len < sizeof(source)) {
		memcpy(source, h->source, h->source_len);
		source[h->source_len] = '\0';
		if (same_source(l->opts->path, source)) {
			error_report(&l->c->err, ERR_ERROR, use_loc(l, use),
					"a module cannot import the file being compiled");
			l->failed = 1;
			return 1;
		}
	}
	if (duplicate_module(&l->c->sema, h)) {
		error_report(&l->c->err, ERR_ERROR, use_loc(l, use),
				"another imported module uses module id '%.*s'",
				(int)h->mod_len, h->mod);
		l->failed = 1;
		return 1;
	}
	return 0;
}

static uint32_t module_new (ImportLoader *l, const char *name, uint16_t len)
{
	Sema *s = &l->c->sema;

	if (s->module_count >= s->module_cap) {
		uint32_t old = s->module_cap;
		s->module_cap = old ? old << 1 : 8;
		s->modules = arena_realloc(s->arena, s->modules,
				sizeof(ImportModule) * old, sizeof(ImportModule) * s->module_cap);
	}
	uint32_t idx = s->module_count++;
	ImportModule *m = &s->modules[idx];

	memset(m, 0, sizeof(*m));
	m->name = name;
	m->name_len = len;
	m->init_node = NO_NODE;
	return idx;
}

static int add_alias (ImportLoader *l, uint32_t module,
		const char *name, uint16_t len, uint32_t use)
{
	Sema *s = &l->c->sema;

	for (uint32_t i = 0; i < s->alias_count; i++) {
		ImportAlias *a = &s->aliases[i];

		if (a->len == len && memcmp(a->name, name, len) == 0) {
			error_report(&l->c->err, ERR_ERROR, use_loc(l, use),
					"module qualifier '%.*s' is already in use", (int)len, name);
			l->failed = 1;
			return 1;
		}
	}
	if (s->alias_count >= s->alias_cap) {
		uint32_t old = s->alias_cap;
		s->alias_cap = old ? old << 1 : 8;
		s->aliases = arena_realloc(s->arena, s->aliases,
				sizeof(ImportAlias) * old, sizeof(ImportAlias) * s->alias_cap);
	}
	s->aliases[s->alias_count++] = (ImportAlias){ name, len, module };
	return 0;
}

static int add_binding (ImportLoader *l, uint32_t module, const char *name,
		uint16_t len, ImportedDecl *item, uint32_t use)
{
	Sema *s = &l->c->sema;

	for (uint32_t i = 0; i < s->binding_count; i++) {
		ImportBinding *b = s->bindings + i;

		if (b->len == len && memcmp(b->name, name, len) == 0) {
			error_report(&l->c->err, ERR_ERROR, use_loc(l, use),
					"imported name '%.*s' is already bound", (int)len, name);
			l->failed = 1;
			return 1;
		}
	}
	if (s->binding_count >= s->binding_cap) {
		uint32_t old = s->binding_cap;
		s->binding_cap = old ? old << 1 : 8;
		s->bindings = arena_realloc(s->arena, s->bindings,
				sizeof(ImportBinding) * old, sizeof(ImportBinding) * s->binding_cap);
	}
	s->bindings[s->binding_count++] = (ImportBinding){ name, len, item, module, use };
	return 0;
}

static void join_name (ImportLoader *l, const ModName *prefix, const char *seg, uint16_t slen, ModName *out)
{
	uint32_t total = (uint32_t)prefix->len + (prefix->len ? 2u : 0u) + slen;
	uint32_t at = 0;
	char *buf = arena_alloc(&l->c->arena, (size_t)total + 1);

	if (prefix->len) {
		memcpy(buf, prefix->str, prefix->len);
		at = prefix->len;
		buf[at++] = ':';
		buf[at++] = ':';
	}
	memcpy(buf + at, seg, slen);
	buf[total] = '\0';
	out->str = buf;
	out->len = (uint16_t)total;
}

static char *copy_name (ImportLoader *l, const char *s, uint16_t len)
{
	char *out = arena_alloc(&l->c->arena, (size_t)len + 1);

	memcpy(out, s, len);
	out[len] = '\0';
	return out;
}

static int entry_cmp (const void *a, const void *b)
{
	const FolderEntry *x = a;
	const FolderEntry *y = b;

	return strcmp(x->name, y->name);
}

static int entry_kind (ImportLoader *l, const char *dir, const char *name,
		uint16_t len, char **full)
{
	struct stat st;

	*full = make_path(l, dir, name, len, "");
	if (stat(*full, &st) != 0) return -1;
	if (S_ISDIR(st.st_mode)) return 1;
	if (S_ISREG(st.st_mode) && len > 4 && !strcmp(name + len - 4, ".r2i"))
		return 0;
	return -1;
}

static FolderEntry *collect_entries (ImportLoader *l, const char *dir, uint32_t use, uint32_t *count)
{
	uint32_t cap = 8;
	uint32_t n = 0;
	FolderEntry *out = arena_alloc(&l->c->arena, sizeof(FolderEntry) * cap);
	DIR *d = opendir(dir);

	if (!d) {
		error_report(&l->c->err, ERR_ERROR, use_loc(l, use),
				"cannot read module folder '%s'", dir);
		l->failed = 1;
		return NULL;
	}
	struct dirent *de;

	while ((de = readdir(d)) != NULL) {
		const char *name = de->d_name;
		uint16_t len = (uint16_t)strlen(name);
		char *full;
		int kind;

		if (!len || name[0] == '.') continue;
		kind = entry_kind(l, dir, name, len, &full);
		if (kind < 0) continue;
		if (n >= cap) {
			uint32_t old = cap;

			cap <<= 1;
			out = arena_realloc(&l->c->arena, out, sizeof(FolderEntry) * old,
					sizeof(FolderEntry) * cap);
		}
		out[n].name = copy_name(l, name, len);
		out[n].is_dir = (uint8_t)kind;
		n++;
	}
	closedir(d);
	*count = n;
	return out;
}

static void load_folder (ImportLoader *l, const char *path, const FolderLoad *fl)
{
	if (fl->depth > FOLDER_MAX_DEPTH) {
		error_report(&l->c->err, ERR_ERROR, use_loc(l, fl->use),
				"module folder is nested too deeply");
		l->failed = 1;
		return;
	}
	uint32_t count = 0;
	FolderEntry *entries = collect_entries(l, path, fl->use, &count);

	if (!entries) return;
	qsort(entries, count, sizeof(FolderEntry), entry_cmp);
	ModName ap = fl->alias.len ? fl->alias : fl->canon;

	for (uint32_t i = 0; i < count; i++) {
		const char *name = entries[i].name;
		uint16_t nlen = (uint16_t)strlen(name);
		uint16_t stem_len = entries[i].is_dir ? nlen : (uint16_t)(nlen - 4);
		ModName mod = {0};
		ModName alias_mod = {0};
		char *full = make_path(l, path, name, nlen, "");

		join_name(l, &fl->canon, name, stem_len, &mod);
		join_name(l, &ap, name, stem_len, &alias_mod);
		if (entries[i].is_dir) {
			FolderLoad next = { mod, alias_mod, fl->use, fl->depth + 1 };

			load_folder(l, full, &next);
			continue;
		}
		ImportRequest req = { .name = mod.str, .len = mod.len,
				.alias = alias_mod.str, .alias_len = alias_mod.len,
				.path = full, .use = fl->use, .direct = 1 };

		(void)load_module(l, &req);
	}
}

static void load_from (ImportLoader *l, uint32_t idx)
{
	AST *ast = &l->c->parser.ast;
	ASTNode *n = ast->nodes + idx;
	uint32_t mod = NO_MODULE;

	for (uint32_t item = n->child; item != NO_NODE; item = ast->nodes[item].next_bro) {
		ASTNode *it = ast->nodes + item;
		uint32_t local = it->child != NO_NODE ? it->child : item;
		const char *lname = ast->nodes[local].str;
		uint16_t llen = ast->nodes[local].len;
		ModName sub = {0};
		ModName base = { n->str, n->len };

		join_name(l, &base, it->str, it->len, &sub);
		if (find_in_dirs(l, sub.str, sub.len, &FIND_FILE)
				|| find_in_dirs(l, sub.str, sub.len, &FIND_DIR)) {
			if (find_in_dirs(l, n->str, n->len, &FIND_FILE))
				error_report(&l->c->err, ERR_WARNING, use_loc(l, item),
						"'%.*s' resolves to submodule '%.*s'; a symbol with that name"
						" exported by module '%.*s' is ignored",
						(int)it->len, it->str, (int)sub.len, sub.str,
						(int)n->len, n->str);
			ImportRequest req = { .name = sub.str, .len = sub.len, .alias = lname,
					.alias_len = llen, .use = item, .direct = 1 };

			(void)load_path(l, &req);
			continue;
		}
		if (mod == NO_MODULE
				&& find_in_dirs(l, n->str, n->len, &FIND_FILE)) {
			ImportRequest req = { .name = n->str, .len = n->len,
					.use = idx, .direct = 1 };

			mod = load_module(l, &req);
			if (mod == NO_MODULE) continue;
		}
		if (mod == NO_MODULE) {
			error_report(&l->c->err, ERR_ERROR, use_loc(l, item),
					"module '%.*s' does not export '%.*s'",
					(int)n->len, n->str, (int)it->len, it->str);
			l->failed = 1;
			continue;
		}
		ImportedDecl *decl = import_find_item(&l->c->sema.modules[mod], it->str, it->len);

		if (!decl) {
			error_report(&l->c->err, ERR_ERROR, use_loc(l, item),
					"module '%.*s' does not export '%.*s'",
					(int)n->len, n->str, (int)it->len, it->str);
			l->failed = 1;
			continue;
		}
		add_binding(l, mod, lname, llen, decl, item);
	}
}

static void load_import (ImportLoader *l, uint32_t idx)
{
	ASTNode *n = l->c->parser.ast.nodes + idx;
	const char *alias = n->str;
	uint16_t len = n->len;

	if (n->child != NO_NODE) {
		ASTNode *a = l->c->parser.ast.nodes + n->child;

		alias = a->str;
		len = a->len;
	}
	ImportRequest req = { .name = n->str, .len = n->len, .alias = alias,
			.alias_len = len, .use = idx, .direct = 1 };

	(void)load_path(l, &req);
}

static void load_deps (ImportLoader *l, const R2iHeader *h, uint32_t use)
{
	for (uint16_t i = 0; i < h->dep_count; i++) {
		ImportRequest req = { .name = h->deps[i], .len = h->dep_lens[i],
				.use = use, .direct = 0 };

		(void)load_path(l, &req);
	}
}

static uint32_t reuse_module (ImportLoader *l, uint32_t idx, const ImportRequest *req)
{
	Sema *s = &l->c->sema;

	if (s->modules[idx].loading) {
		error_report(&l->c->err, ERR_ERROR, use_loc(l, req->use),
				"import cycle while loading module '%.*s'",
				(int)req->len, req->name);
		l->failed = 1;
		return NO_MODULE;
	}
	s->modules[idx].direct |= (uint8_t)req->direct;
	if (req->alias && add_alias(l, idx, req->alias, req->alias_len, req->use))
		return NO_MODULE;
	return idx;
}

static uint32_t open_module (ImportLoader *l, const ImportRequest *req, char **src, R2iHeader *h)
{
	const char *path = req->path ? req->path : find_in_dirs(l, req->name, req->len, &FIND_FILE);

	if (!path) {
		error_report(&l->c->err, ERR_ERROR, use_loc(l, req->use),
				"cannot find module '%.*s' (.r2i not found in the source"
				" directory, the modules directory or a -p path)",
				(int)req->len, req->name);
		l->failed = 1;
		return NO_MODULE;
	}
	*src = read_module_file(l, path);
	if (!*src) {
		l->failed = 1;
		return NO_MODULE;
	}
	if (parse_header(l, path, *src, h) || check_module(l, h, req->use)) {
		l->failed = 1;
		return NO_MODULE;
	}
	uint32_t idx = module_new(l, req->name, req->len);
	ImportModule *m = l->c->sema.modules + idx;

	m->path = (char *)path;
	m->mod = h->mod;
	m->mod_len = (uint16_t)h->mod_len;
	m->has_init = (uint8_t)h->has_init;
	m->direct = (uint8_t)req->direct;
	if (req->alias && add_alias(l, idx, req->alias, req->alias_len, req->use))
		return NO_MODULE;
	return idx;
}

static uint32_t load_module (ImportLoader *l, const ImportRequest *req)
{
	Sema *s = &l->c->sema;
	uint32_t idx = import_find_module(s, req->name, req->len);
	if (idx != NO_MODULE) return reuse_module(l, idx, req);

	R2iHeader h;
	char *src;
	idx = open_module(l, req, &src, &h);
	if (idx == NO_MODULE) return NO_MODULE;

	s->modules[idx].loading = 1;
	if (import_parse_body(l, idx, src, s->modules[idx].path, req->use)) {
		s->modules[idx].loading = 0;
		return NO_MODULE;
	}
	load_deps(l, &h, req->use);
	s->modules[idx].loading = 0;
	s->modules[idx].loaded = 1;
	return idx;
}

static uint32_t load_path (ImportLoader *l, const ImportRequest *req)
{
	const char *dir = find_in_dirs(l, req->name, req->len, &FIND_DIR);

	if (dir) {
		ModName name = { req->name, req->len };
		ModName alias = { req->alias, req->alias ? req->alias_len : 0 };
		FolderLoad fl = { name, alias, req->use, 0 };

		load_folder(l, dir, &fl);
		return NO_MODULE;
	}
	if (find_in_dirs(l, req->name, req->len, &FIND_ANY_DIR)) {
		error_report(&l->c->err, ERR_ERROR, use_loc(l, req->use),
				"module folder '%.*s' contains no .r2i files",
				(int)req->len, req->name);
		l->failed = 1;
		return NO_MODULE;
	}
	ImportRequest file = *req;

	file.path = NULL;
	return load_module(l, &file);
}

static uint32_t *collect_import_nodes (Compiler *c, uint32_t *count)
{
	AST *ast = &c->parser.ast;
	uint32_t cap = 8;
	uint32_t n = 0;
	uint32_t *out = arena_alloc(&c->arena, sizeof(uint32_t) * cap);

	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		uint8_t type = ast->nodes[idx].type;

		if (type != NODE_IMPORT_DEC && type != NODE_FROM_DEC) continue;
		if (n >= cap) {
			uint32_t old = cap;

			cap <<= 1;
			out = arena_realloc(&c->arena, out, sizeof(uint32_t) * old,
					sizeof(uint32_t) * cap);
		}
		out[n++] = idx;
	}
	*count = n;
	return out;
}

int imports_load (Compiler *c, const CompilerOpts *opts)
{
	ImportLoader l = { c, opts, 0 };
	uint32_t count = 0;
	uint32_t *nodes = collect_import_nodes(c, &count);

	for (uint32_t i = 0; i < count; i++) {
		ASTNode *n = &c->parser.ast.nodes[nodes[i]];

		if (n->type == NODE_IMPORT_DEC) load_import(&l, nodes[i]);
		else if (n->type == NODE_FROM_DEC) load_from(&l, nodes[i]);
	}
	return l.failed;
}
