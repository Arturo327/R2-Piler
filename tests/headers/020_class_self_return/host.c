#include "include/chain.h"

int main (void)
{
	C c;

	_r2_o_5chain_1C_init(&c, 4);
	if (_r2_m_5chain_1C_3set(&c, 9) != &c) return 1;
	if (_r2_m_5chain_1C_3get(&c) != 9) return 2;
	return 0;
}
