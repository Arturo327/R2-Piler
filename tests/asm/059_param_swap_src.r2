// OPT: PARAM/ARG swap. f(a, b) calls g(b, a); f and g both contain a CALL,
// so the inliner leaves them intact at -O2/-O3 and the ARG group really has
// to permute two argument registers.
// f(1, 2) -> g(2, 1) -> h(3, 1) = 3 * 3 + 1 = 10.
fn h(a : i64, b : i64) : i64 {
	return a * 3 + b;
}
fn g(a : i64, b : i64) : i64 {
	return h(a + b, a - b);
}
fn f(a : i64, b : i64) : i64 {
	return g(b, a);
}
fn main() : i64 {
	if (f(1, 2) == 10) {
		return 0;
	}
	return 1;
}
