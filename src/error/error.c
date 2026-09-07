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
	if (level == ERR_WARNING) return "warning";
	return "error";
}

void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...)
{
	fprintf(stderr, "%s:%d:%d: %s: ", er->file, loc.line, loc.col, level_str(level));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	if (level == ERR_ERROR) er->err_count++;
	else er->warn_count++;
}
