#include "include/vec.h"

int main ()
{
	Vec a(3, 4);
	Vec b(1, 2);
	Vec c = a + b;

	if (!(c == Vec(4, 6))) return 1;
	if (*c[0] != 4 || *c[1] != 6) return 2;
	if (c.dot(Vec(1, 1)) != 10) return 3;
	if (Vec::origen().dot(Vec(1, 1)) != 0) return 4;
	if (!(-a == Vec(-3, -4))) return 5;
	if (a.suma(b).y != 6) return 6;
	a.destroy();
	return 0;
}
