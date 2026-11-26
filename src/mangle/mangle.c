#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mangle/mangle.h"

#define PATH_MAX 4096

static int segment_char_ok (unsigned char c)
{
	return c == '_' || c == '.' || (c >= '0' && c <= '9')
			|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static size_t escaped_len (const char *s, size_t len)
{
	size_t out = 0;

	for (size_t i = 0; i < len; i++)
		out += segment_char_ok((unsigned char)s[i]) ? 1 : 3;
	return out;
}

static size_t escape_segment (char *out, const char *s, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	size_t at = 0;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];

		if (segment_char_ok(c)) {
			out[at++] = (char)c;
		} else {
			out[at++] = '$';
			out[at++] = hex[c >> 4];
			out[at++] = hex[c & 15];
		}
	}
	return at;
}

static size_t common_root_len (const char *a, const char *b)
{
	size_t n = 0;

	while (a[n] && b[n] && a[n] == b[n]) n++;
	if (a[n] == '\0' && b[n] == '/') return n;
	if (b[n] == '\0' && a[n] == '/') return n;
	while (n > 0 && a[n - 1] != '/') n--;
	return n > 1 ? n - 1 : 0;
}

static const char *module_path (const char *full, const char *cwd)
{
	size_t root = common_root_len(cwd, full);

	if (root == 0) {
		const char *slash = strrchr(full, '/');

		return slash ? slash + 1 : full;
	}
	return full + root + 1;
}

static int canonical_path (const char *path, char *out, size_t cap)
{
	char resolved[PATH_MAX];
	const char *base = strrchr(path, '/');

	if (!realpath(path, resolved)) return 0;
	base = base ? base + 1 : path;
	const char *slash = strrchr(resolved, '/');
	size_t dir_len = slash ? (size_t)(slash - resolved) : 0;
	int n = snprintf(out, cap, "%.*s/%s", (int)dir_len, resolved, base);
	return n > 0 && (size_t)n < cap;
}

static char *encode_module (Arena *arena, const char *rel, size_t len)
{
	size_t cap = len * 4 + 64;
	char *buf = arena_alloc(arena, cap);
	const char *seg = rel;
	const char *end = rel + len;
	size_t at = 0;

	while (seg < end) {
		const char *slash = memchr(seg, '/', (size_t)(end - seg));
		const char *stop = slash ? slash : end;
		size_t seg_len = (size_t)(stop - seg);

		at += (size_t)snprintf(buf + at, cap - at, "%zu",
				escaped_len(seg, seg_len));
		at += escape_segment(buf + at, seg, seg_len);
		if (!slash) break;
		seg = slash + 1;
	}
	buf[at] = '\0';
	return buf;
}

int mangle_module_relative (const char *path, char *out, size_t cap)
{
	size_t len = strlen(path);
	char full[PATH_MAX];
	char cwd[PATH_MAX];

	if (len < 4 || strcmp(path + len - 3, ".r2") != 0) {
		fprintf(stderr, "Error: source file '%s' must end in '.r2'\n", path);
		return 1;
	}
	if (!canonical_path(path, full, sizeof(full)) || !getcwd(cwd, sizeof(cwd))) {
		fprintf(stderr, "Error: cannot resolve the path of '%s'\n", path);
		return 1;
	}

	const char *rel = module_path(full, cwd);
	size_t rel_len = strlen(rel);

	if (rel_len < 3 || strcmp(rel + rel_len - 3, ".r2") != 0) {
		fprintf(stderr, "Error: source file '%s' must end in '.r2'\n", path);
		return 1;
	}
	if (rel_len >= cap) {
		fprintf(stderr, "Error: source path '%s' is too long\n", path);
		return 1;
	}
	memcpy(out, rel, rel_len + 1);
	return 0;
}

int mangle_module (Arena *arena, const char *path, const char **out)
{
	char rel[PATH_MAX];

	if (mangle_module_relative(path, rel, sizeof(rel))) return 1;
	size_t rel_len = strlen(rel);

	*out = encode_module(arena, rel, rel_len - 3);
	return 0;
}

int mangle_module_name (Arena *arena, const char *name, uint16_t len, const char **out)
{
	if (!len) return 1;

	size_t cap = (size_t)len * 4 + 32;
	char *buf = arena_alloc(arena, cap);
	int n = snprintf(buf, cap, "%zu", escaped_len(name, len));

	if (n <= 0) return 1;
	n += (int)escape_segment(buf + n, name, len);
	buf[n] = '\0';
	*out = buf;
	return 0;
}

char *mangle_symbol (Arena *arena, const MangleName *m)
{
	size_t cap = strlen(m->kind) + strlen(m->mod) + (size_t)m->alen + m->blen + 64;
	char *out = arena_alloc(arena, cap);
	size_t at = (size_t)snprintf(out, cap, "_r2_%s_%s_%u%.*s", m->kind, m->mod,
			(unsigned)m->alen, (int)m->alen, m->a);

	if (m->b) at += (size_t)snprintf(out + at, cap - at, "_%u%.*s",
			(unsigned)m->blen, (int)m->blen, m->b);
	if (m->dup) snprintf(out + at, cap - at, ".%u", (unsigned)m->dup);
	return out;
}

char *mangle_class_special (Arena *arena, const char *mod, const char *cls,
		uint16_t clen, const char *canon)
{
	size_t cap = strlen(mod) + strlen(canon) + (size_t)clen + 32;
	char *out = arena_alloc(arena, cap);

	snprintf(out, cap, "_r2_o_%s_%u%.*s_%s", mod, (unsigned)clen,
			(int)clen, cls, canon);
	return out;
}

char *mangle_init (Arena *arena, const char *mod)
{
	size_t cap = strlen(mod) + 16;
	char *out = arena_alloc(arena, cap);

	snprintf(out, cap, "__r2_init_%s", mod);
	return out;
}

char *mangle_plain (Arena *arena, const char *name, uint16_t len)
{
	char *out = arena_alloc(arena, (size_t)len + 1);

	memcpy(out, name, len);
	out[len] = '\0';
	return out;
}
