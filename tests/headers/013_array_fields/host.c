#include "include/a.h"
int main (void) { S s = {{1, 2, 3}, {4, 5}}; return sum(&s) == 15 ? 0 : 1; }
