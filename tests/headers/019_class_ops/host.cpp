#include "include/acc.h"

int main ()
{
	Acc a(1);
	Acc b(2);

	a = b;
	if (a.x != 2) return 1;
	a += b;
	if (a.x != 14) return 2;
	++a;
	if (a.x != 114) return 3;
	if (a != 114) return 4;
	return 0;
}
