#ifndef IMPORTS_H
#define IMPORTS_H

#include <stdint.h>

typedef struct ImportedDecl {
	const char *name;
	uint16_t len;
	uint32_t node;
} ImportedDecl;

typedef struct ImportModule {
	const char *name;
	uint16_t name_len;
	char *path;
	const char *mod;
	uint16_t mod_len;
	uint32_t init_node;
	ImportedDecl *items;
	uint16_t item_count;
	uint16_t item_cap;
	uint8_t has_init;
	uint8_t direct;
	uint8_t loading;
	uint8_t loaded;
} ImportModule;

typedef struct ImportAlias {
	const char *name;
	uint16_t len;
	uint32_t module;
} ImportAlias;

typedef struct ImportBinding {
	const char *name;
	uint16_t len;
	ImportedDecl *item;
	uint32_t module;
	uint32_t node;
} ImportBinding;

struct Compiler;
struct CompilerOpts;

int imports_load (struct Compiler *c, const struct CompilerOpts *opts);

#endif
