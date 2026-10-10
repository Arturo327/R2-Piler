// OPT: FULL-only CSE (MUL) across the call boundary. `a = x * y` and
// `b = x * y` share srcs, so O2 rewrites the second MUL to a MOVE while O1
// keeps two `imulq` per call. 1999999 calls make O1 > O2 in Ir.
// s = 14 * sum 1..1999999 = 27999986000000.
fn f(x : i64, y : i64) : i64 {
	var a : i64 = x * y;
	var b : i64 = x * y;
	return a + b;
}
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 1; i < 2000000; i = i + 1) {
		s = s + f(i, 7);
	}
	if (s == 27999986000000) {
		return 0;
	}
	return 1;
}
