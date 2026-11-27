#include "include/hostm.h"

int main ()
{
	if (sizeof(_r2_s_3dep_1D) != 8) return 1;
	if (offsetof(_r2_s_3dep_1D, v) != 0) return 2;
	return 0;
}
