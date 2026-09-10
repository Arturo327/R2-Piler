#include <stdio.h>
#include <stdarg.h>

#include "error/error.h"

#define COL_RED "\033[1;31m"
#define COL_PUR "\033[1;35m"
#define COL_RESET "\033[0m"

void error_init (ErrorReporter *er, const char *file)
{
	er->file = file;
	er->err_count = 0;
	er->warn_count = 0;
	er->line_starts = NULL;
	er->line_count = 0;
}

void error_index_lines (ErrorReporter *er, char *src, Arena *arena)
{
	int count = 1;
	for (char *c = src; *c != '\0'; c++) {
		if (*c == '\n') count++;
	}

	er->line_starts = arena_alloc(arena, sizeof(char *) * (size_t)count);
	er->line_count = count;

	er->line_starts[0] = src;
	int line = 1;
	for (char *c = src; *c != '\0'; c++) {
		if (*c == '\n') {
			er->line_starts[line] = c + 1;
			line++;
		}
	}
}

static const char *level_str (ErrorLevel level, char **col)
{
	if (level == ERR_WARNING) {
		*col = COL_PUR;
		return COL_PUR "WARNING" COL_RESET;
	}

	*col = COL_RED;
	return COL_RED "ERROR" COL_RESET;
}

static char *error_get_line_start (ErrorReporter *er, int line)
{
	int idx = line - 1;
	if (idx < 0) idx = 0;
	if (idx >= er->line_count) idx = er->line_count - 1;
	return er->line_starts[idx];
}

static int error_line_len (const char *line_start)
{
	const char *c = line_start;
	while (*c != '\n' && *c != '\0') c++;
	return (int)(c - line_start);
}

void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...)
{
	char *color;
	fprintf(stderr, "%s:%d:%d: %s: ", er->file, loc.line, loc.col, level_str(level, &color));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	char *line_start = error_get_line_start(er, loc.line);
	int line_len = error_line_len(line_start);
	int col = loc.col - 1;
	int span = loc.len;
	if (col < 0) col = 0;
	if (span < 1) span = 1;
	if (col > line_len) col = line_len;
	if (col + span > line_len) span = line_len - col;

	fprintf(stderr, "   %4d | ", loc.line);
	fprintf(stderr, "%.*s", col, line_start);
	fprintf(stderr, "%s%.*s%s", color, span, line_start + col, COL_RESET);
	fprintf(stderr, "%.*s\n", line_len - col - span, line_start + col + span);

	fprintf(stderr, "        | %*s", col, "");
	fprintf(stderr, "%s^", color);
	for (int i = 1; i < span; i++) fprintf(stderr, "~");
	fprintf(stderr, "%s\n", COL_RESET);

	if (level == ERR_ERROR) er->err_count++;
	else er->warn_count++;
}
