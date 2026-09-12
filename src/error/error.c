#include <stdio.h>
#include <stdarg.h>

#include "error/error.h"

#define COL_RED "\033[1;31m"
#define COL_PUR "\033[1;35m"
#define COL_RESET "\033[0m"
#define TAB_WIDTH 8

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

static int char_vis_width (char c, int vis)
{
	if (c == '\t')
		return TAB_WIDTH - (vis % TAB_WIDTH);
	if (c == '\r')
		return 0;
	return 1;
}

static int vis_col_upto (const char *s, int nbytes, int vis)
{
	for (int i = 0; i < nbytes; i++)
		vis += char_vis_width(s[i], vis);
	return vis;
}

static void print_expanded (const char *s, int len, int *vis)
{
	for (int i = 0; i < len; i++) {
		char c = s[i];
		if (c == '\t') {
			int w = TAB_WIDTH - (*vis % TAB_WIDTH);
			for (int i = 0; i < w; i++) fputc(' ', stderr);
			*vis += w;
		} else if (c != '\r') {
			fputc(c, stderr);
			*vis += 1;
		}
	}
}

static void print_snippet (const char *line, int line_len, int col, int span,
		int line_no, const char *color)
{
	int vis = 0;

	fprintf(stderr, "   %4d | ", line_no);
	print_expanded(line, col, &vis);
	fprintf(stderr, "%s", color);
	print_expanded(line + col, span, &vis);
	fprintf(stderr, "%s", COL_RESET);
	print_expanded(line + col + span, line_len - col - span, &vis);
	fprintf(stderr, "\n");
}

static void print_caret (int vis_start, int vis_width, const char *color)
{
	fprintf(stderr, "        | ");
	for (int i = 0; i < vis_start; i++) fputc(' ', stderr);
	fprintf(stderr, "%s^", color);
	for (int i = 1; i < vis_width; i++)
		fputc('~', stderr);
	fprintf(stderr, "%s\n", COL_RESET);
}

void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...)
{
	char *color;
	char *line_start;
	int line_len, col, span, vis_start, vis_end;

	fprintf(stderr, "%s:%d:%d: %s: ", er->file, loc.line, loc.col, level_str(level, &color));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	line_start = error_get_line_start(er, loc.line);
	line_len = error_line_len(line_start);
	col = loc.col - 1;
	span = loc.len;
	if (col < 0) col = 0;
	if (span < 1) span = 1;
	if (col > line_len) col = line_len;
	if (col + span > line_len) span = line_len - col;

	vis_start = vis_col_upto(line_start, col, 0);
	vis_end = vis_col_upto(line_start + col, span, vis_start);
	if (vis_end <= vis_start)
		vis_end = vis_start + 1;

	print_snippet(line_start, line_len, col, span, loc.line, color);
	print_caret(vis_start, vis_end - vis_start, color);

	if (level == ERR_ERROR) er->err_count++;
	else er->warn_count++;
}
