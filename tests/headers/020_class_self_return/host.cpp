#include "include/chain.h"

int main ()
{
	C c(4);

	if (c.set(9) != &c) return 1;
	if (c.get() != 9) return 2;
	return 0;
}
