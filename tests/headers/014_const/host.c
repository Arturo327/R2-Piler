#include "include/a.h"

int main (void) {
	C c = { .f = 1, .p = &g };
	__r2_init_1a();
	return (int)(g + *cp + *take(4, &g) + get() + c.f
			+ (int)sizeof(c.items)) == 22 ? 0 : 1;
}
