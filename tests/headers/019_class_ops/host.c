#include "include/acc.h"

int main (void)
{
	Acc a, b;

	_r2_o_3acc_3Acc_init(&a, 1);
	_r2_o_3acc_3Acc_init(&b, 2);
	_r2_o_3acc_3Acc_assign(&a, b);
	if (a.x != 2) return 1;
	_r2_o_3acc_3Acc_add_assign(&a, b);
	if (a.x != 14) return 2;
	_r2_o_3acc_3Acc_inc(&a);
	if (a.x != 114) return 3;
	if (_r2_o_3acc_3Acc_ne(&a, 114)) return 4;
	return 0;
}
