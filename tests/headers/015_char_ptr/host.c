#pragma GCC diagnostic error "-Wpointer-sign"
#include <stddef.h>
#include "include/chars.h"

int main (void) {
	struct Str st = { .s = "hi", .buf = "ab", .n = 3 };
	char buf[8] = { 0 };
	const char *g = greeting;
	const char *r = ret();

	if (st.s[0] != 'h' || st.buf[0] != 'a') return 1;
	if (first(st.s) != 'h') return 2;
	if (first("hi") != 'h') return 3;
	if (touch(buf) != 'A' || buf[0] != 'A') return 4;
	if (touch(st.buf) != 'A' || st.buf[0] != 'A') return 5;
	if (touch(bytes) != 'A' || bytes[0] != 'A') return 6;
	if (slice.len[0] != 0 || slice.data != NULL) return 7;
	if (g != NULL) return 8;
	if (r != NULL) return 9;
	return 0;
}
