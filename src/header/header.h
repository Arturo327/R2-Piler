#ifndef HEADER_H
#define HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "ir/ir.h"
#include "sema/sema.h"

typedef struct CHeader {
	IR *ir;
	Sema *sema;
	const char *source;
	const char *dir;
	char *canon;
	char *path;
	char *guard;
	char *init_macro;
	char *init_sym;

	const char **type_names;
	uint16_t type_total;
	uint16_t *decl_nodes;
	uint8_t *type_used;
	uint8_t *type_done;

	TypeId *fn_types;
	uint8_t *fn_seen;
	uint16_t fn_count;

	TypeId *slice_types;
	uint8_t *slice_seen;
	uint16_t slice_count;
	uint8_t any_used;

	char *code;
	size_t len;
	size_t cap;
	uint8_t ready;
} CHeader;

int header_generate (CHeader *h, IR *ir, Sema *sema,
		const char *source, const char *dir);
int header_write (CHeader *h);
void header_free (CHeader *h);

#endif
