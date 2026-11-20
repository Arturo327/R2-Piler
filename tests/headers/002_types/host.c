#include "include/a.h"
int main (void) {
	Punto p; U u; Bits b; Small s; Mixta m;
	p.x = 20; p.y = 22;
	u.big = 4;
	b.d = 5;
	s.x = 1; s.y = 2;
	m.inner = p; m.p = &p;
	(void)m;
	return p.x + p.y + (int)u.big + (int)b.d + s.x + s.y
		+ (int)Color_Verde + (int)sizeof(Entero) + (int)sizeof(Manzana)
		== 68 ? 0 : 1;
}
