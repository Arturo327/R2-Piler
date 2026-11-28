#include "util/strbuf.h"

#include <stdio.h>
#include <stdlib.h>

#include "imports/imports.h"
#include "sema/sema.h"

void sb_vprintf (char **code, size_t *len, size_t *cap, const char *fmt, va_list ap)
{
	va_list probe;

	va_copy(probe, ap);
	int need = vsnprintf(NULL, 0, fmt, probe);
	va_end(probe);
	if (need < 0) return;
	if (*len + (size_t)need + 1 > *cap) {
		size_t next = *cap;

		while (*len + (size_t)need + 1 > next) next <<= 1;
		char *grown = realloc(*code, next);

		if (!grown) {
			fprintf(stderr, "Not enough memory\n");
			exit(1);
		}
		*code = grown;
		*cap = next;
	}
	vsnprintf(*code + *len, (size_t)need + 1, fmt, ap);
	*len += (size_t)need;
}

void sb_printf (char **code, size_t *len, size_t *cap, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	sb_vprintf(code, len, cap, fmt, ap);
	va_end(ap);
}

void sb_indent (char **code, size_t *len, size_t *cap, int depth)
{
	for (int i = 0; i < depth; i++)
		sb_printf(code, len, cap, "\t");
}

void sb_print_meta (char **code, size_t *len, size_t *cap, const struct Sema *s, int blank)
{
	sb_printf(code, len, cap, "// depends:");
	for (uint16_t i = 0; i < s->dep_count; i++)
		sb_printf(code, len, cap, "%s%.*s", i ? ", " : " ",
				(int)s->dep_lens[i], s->deps[i]);
	sb_printf(code, len, cap, "\n// total_imports:");
	int first = 1;
	for (uint32_t i = 0; i < s->module_count; i++) {
		const ImportModule *m = s->modules + i;

		sb_printf(code, len, cap, "%s%.*s", first ? " " : ", ",
				(int)m->name_len, m->name);
		first = 0;
	}
	sb_printf(code, len, cap, blank ? "\n\n" : "\n");
}
