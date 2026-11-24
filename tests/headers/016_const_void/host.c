#include <stddef.h>
#include "include/cvoid.h"

int main (void) {
	int x = 5;
	struct Buf b = { .p = &x, .n = 1 };
	const void *p = peek(&x);
	void *q = fill(&x, 1);
	const void *g = ro;

	if (b.p != &x) return 1;
	if (p != &x) return 2;
	if (q != &x) return 3;
	if (g != NULL) return 4;
	return 0;
}
