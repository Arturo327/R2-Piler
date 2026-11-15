#include <stdio.h>
#include <string.h>

#include "mangle/mangle.h"

static int segment_ok (const char *s, size_t len)
{
	if (!len || (s[0] >= '0' && s[0] <= '9')) return 0;
	for (size_t i = 0; i < len; i++) {
		char c = s[i];
		int ok = c == '_' || (c >= '0' && c <= '9')
				|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');

		if (!ok) return 0;
	}
	return 1;
}

int mangle_module (Arena *arena, const char *path, const char **out)
{
	while (path[0] == '.' && path[1] == '/') path += 2;
	size_t len = strlen(path);
	if (len < 4 || strcmp(path + len - 3, ".r2") != 0) {
		fprintf(stderr, "Error: source file '%s' must end in '.r2'\n", path);
		return 1;
	}

	const char *seg = path;
	const char *end = path + len - 3;
	char *buf = arena_alloc(arena, 2 * len + 1);
	size_t at = 0;
	while (seg <= end) {
		const char *slash = memchr(seg, '/', (size_t)(end - seg));
		size_t n = (size_t)((slash ? slash : end) - seg);

		if (!segment_ok(seg, n)) {
			fprintf(stderr, "Error: '%s': invalid path segment '%.*s' (module paths are"
					" relative and only allow [A-Za-z0-9_], not starting with a digit)\n",
					path, (int)n, seg);
			return 1;
		}
		at += (size_t)sprintf(buf + at, "%zu", n);
		memcpy(buf + at, seg, n);
		at += n;
		seg += n + 1;
	}
	buf[at] = '\0';
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
