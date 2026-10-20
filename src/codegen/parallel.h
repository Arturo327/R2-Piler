#ifndef PARALLEL_H
#define PARALLEL_H

#include <stdint.h>

#define PM_SLOT (-1)

typedef struct PMove {
	int8_t from;
	int8_t to;
	uint8_t size;
	uint8_t sign;
	uint32_t slot;
} PMove;

enum {
	PM_ACT_STORE = 1,
	PM_ACT_MOVE,
	PM_ACT_SWAP,
	PM_ACT_LOAD
};

typedef struct PMAction {
	uint8_t kind;
	int8_t from;
	int8_t to;
	uint8_t size;
	uint8_t sign;
	uint32_t slot;
} PMAction;

uint32_t parallel_actions (PMove *moves, uint32_t count, PMAction *out);

#endif
