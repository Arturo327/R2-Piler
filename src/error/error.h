#ifndef ERROR_H
#define ERROR_H

typedef enum {
	ERR_WARNING,
	ERR_ERROR
} ErrorLevel;

typedef struct ErrorLoc {
	int line;
	int col;
	const char *line_start;
	int len;
} ErrorLoc;

typedef struct ErrorReporter {
	const char *file;
	int err_count;
	int warn_count;
} ErrorReporter;

void error_init (ErrorReporter *er, const char *file);
void error_report (ErrorReporter *er, ErrorLevel level, ErrorLoc loc, const char *fmt, ...);

#endif
