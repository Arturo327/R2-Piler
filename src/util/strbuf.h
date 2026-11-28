#ifndef STRBUF_H
#define STRBUF_H

#include <stdarg.h>
#include <stddef.h>

void sb_vprintf (char **code, size_t *len, size_t *cap, const char *fmt, va_list ap);
void sb_printf (char **code, size_t *len, size_t *cap, const char *fmt, ...);
void sb_indent (char **code, size_t *len, size_t *cap, int depth);

struct Sema;
void sb_print_meta (char **code, size_t *len, size_t *cap, const struct Sema *s, int blank);

#endif
