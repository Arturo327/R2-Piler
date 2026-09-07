#include <stdio.h>
#include <stdarg.h>

#include "error/error.h"

void error_init (ErrorReporter *er, const char *file)
{
	er->file = file;
	er->err_count = 0;
	er->warn_count = 0;
}

static const char *level_str (ErrorLevel level)
{
	if (level == ERR_WARNING) return "\033[1;35mWARNING\033[0m";
	return "\033[1;31mERROR\033[0m";
}

static size_t error_line_len (ErrorLoc loc)
{
	const char *c = loc.line_start;
	while (*c != '\n' && *c != '\0') c++;
	return (size_t)(c - loc.line_start);
}

void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...)
{
	fprintf(stderr, "%s:%d:%d: %s: ", er->file, loc.line, loc.col, level_str(level));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	size_t len = error_line_len(loc);
	fprintf(stderr, "   %4d | ", loc.line);
	fprintf(stderr, "%.*s\n", (int)len, loc.line_start);

	if (loc.col > 1) fprintf(stderr, "        | %*s\033[1;31m^\033[0m\n\n", loc.col - 1, "");
	else fprintf(stderr, "        | \033[1;31m^\033[0m\n\n");

	if (level == ERR_ERROR) er->err_count++;
	else er->warn_count++;
}
