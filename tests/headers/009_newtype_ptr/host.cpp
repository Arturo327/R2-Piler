#include "include/nh.h"
int main () {
	Handle h = make();
	return h == nullptr ? 0 : 1;
}
