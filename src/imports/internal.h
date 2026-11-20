#ifndef IMPORTS_INTERNAL_H
#define IMPORTS_INTERNAL_H

#include <stddef.h>

#include "compiler.h"
#include "imports/imports.h"

#define NO_MODULE 0xFFFFFFFFu

typedef struct R2iHeader {
	const char *source;
	size_t source_len;
	const char *mod;
	size_t mod_len;
	uint16_t has_init;
	const char **deps;
	uint16_t *dep_lens;
	uint16_t dep_count;
} R2iHeader;

typedef struct ImportLoader {
	Compiler *c;
	const CompilerOpts *opts;
	int failed;
} ImportLoader;

uint32_t import_find_module (Sema *s, const char *name, uint16_t len);
ImportedDecl *import_find_item (ImportModule *m, const char *name, uint16_t len);
int import_parse_body (ImportLoader *l, uint32_t module_idx, char *src,
		const char *path, uint32_t use);

#endif
