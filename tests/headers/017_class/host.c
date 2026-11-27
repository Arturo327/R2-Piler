#include "include/vec.h"

int main (void)
{
	Vec a, b, c;

	_r2_o_3vec_3Vec_init(&a, 3, 4);
	_r2_o_3vec_3Vec_init(&b, 1, 2);
	c = _r2_o_3vec_3Vec_add(&a, b);
	if (!_r2_o_3vec_3Vec_eq(&c, (Vec){4, 6})) return 1;
	if (_r2_m_3vec_3Vec_3dot(&a, b) != 11) return 2;
	_r2_o_3vec_3Vec_destroy(&a);
	if (c.x != 4 || c.y != 6) return 3;
	return 0;
}
