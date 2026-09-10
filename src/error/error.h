#ifndef ERROR_H
#define ERROR_H

#include "arena/arena.h"

typedef enum {
	ERR_WARNING,
	ERR_ERROR
} ErrorLevel;

typedef struct ErrorLoc {
	int line;
	int col;
	int len;
} ErrorLoc;

typedef struct ErrorReporter {
	const char *file;
	int err_count;
	int warn_count;
	char **line_starts;
	int line_count;
} ErrorReporter;

void error_init (ErrorReporter *er, const char *file);
void error_index_lines (ErrorReporter *er, char *src, Arena *arena);
void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...);

#endif
