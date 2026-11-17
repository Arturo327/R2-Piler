// Variadic extern functions: '...' in an extern declaration lets the call site
// pass any number of extra arguments (printf/snprintf work end to end).
extern fn printf (fmt : ptr i8, ...) : i32;
extern fn snprintf (buf : ptr i8, n : u64, fmt : ptr i8, ...) : i32;
extern fn strcmp (a : ptr i8, b : ptr i8) : i32;

pub fn main () : i64 {
	arr buf : i8[64];
	var n : i32 = snprintf(buf, 64, "%d-%s-%c-%lld", 42, "ok", 'x', 7);

	if (n != 9) { return 1; }
	if (strcmp(buf, "42-ok-x-7") != 0) { return 2; }
	n = snprintf(buf, 64, "%d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8, 9);
	if (n != 17) { return 3; }
	if (strcmp(buf, "1 2 3 4 5 6 7 8 9") != 0) { return 4; }
	if (printf("variadic ok\n") != 12) { return 5; }
	return 0;
}
