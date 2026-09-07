#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "error/error.h"

void error_init (ErrorReporter *er, const char *file)
{
	er->file = file;
	er->err_count = 0;
	er->warn_count = 0;
}

static const char *level_str (ErrorLevel level)
{
	if (level == ERR_WARNING) return "warning";
	return "error";
}

static const char *get_error_line (ErrorLoc loc)
{
	const char *c = loc.line_start;
	while (*c != '\n') c++;
	size_t len = c - loc.line_start;

	char *str = malloc(len + 1);
	memcpy(str, loc.line_start, len);
	str[len] = '\0';
	return str;
}

void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...)
{
	fprintf(stderr, "%s:%d:%d: %s: ", er->file, loc.line, loc.col, level_str(level));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	const char *error_line = get_error_line(loc);
	if (error_line == NULL) return;
	fprintf(stderr, "   %4d | ", loc.line);
	fprintf(stderr, "%s\n", error_line);
	free((void*)error_line);

	if (loc.col > 1) fprintf(stderr, "        | %*s^\n\n", loc.col - 1, "");
	else fprintf(stderr, "        | ^\n\n");

	if (level == ERR_ERROR) er->err_count++;
	else er->warn_count++;
}
