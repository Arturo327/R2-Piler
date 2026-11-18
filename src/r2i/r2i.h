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

	char *canon;
	char *path;

	char *code;
	size_t len;
	size_t cap;

	uint8_t ready;
} R2i;

int r2i_generate (R2i *r, IR *ir, Sema *sema, const char *source, const char *dir);
int r2i_write (R2i *r);
void r2i_free (R2i *r);

#endif