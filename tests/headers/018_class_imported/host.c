#include "include/hostm.h"

int main (void)
{
	_r2_s_3dep_1D d = {41};

	return (int)(ping(&d) - 42);
}
