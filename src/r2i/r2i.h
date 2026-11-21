#ifndef R2I_H
#define R2I_H

#include <stddef.h>
#include <stdint.h>

#include "ir/ir.h"
#include "sema/sema.h"

typedef struct R2i {
	IR *ir;
	Sema *sema;
	const char *source;
	const char *dir;

	const char **type_syms;
	uint16_t type_total;

	uint8_t *import_used;
	int32_t *type_mod;
	int32_t *node_mod;

	char *canon;
	char *path;

	char *code;
	size_t len;
	size_t cap;

	uint8_t ready;
	uint8_t failed;
} R2i;

int r2i_valid_name (const char *name, size_t len);
int r2i_generate (R2i *r, IR *ir, Sema *sema, const char *source, const char *dir);
int r2i_write (R2i *r);
void r2i_free (R2i *r);

int r2i_build_path (Sema *sema, const char *source, const char *dir,
		const char *ext, char **canon, char **path);
int r2i_make_dirs (const char *dir);
int r2i_make_parent (const char *path);
int r2i_check_owner (const char *path, const char *canon);

#endif
