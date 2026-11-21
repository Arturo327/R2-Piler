#include "include/nh.h"
int main (void) {
	Handle h = make();
	return h == NULL ? 0 : 1;
}
