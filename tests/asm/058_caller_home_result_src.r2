// M2 caller-saved homes: `a` is used after the `g` call and keeps a
// callee-saved home; the call result is only live after its call, so it gets
// a caller-saved home (%r10). O0/O1 spill the temps; O2 has no frame save
// left for the callee home in `f`.
// f(4): a = 8, g(8) = 17, 8 + 3 * 17 + 17 = 76.
fn g(x : i64) : i64 {
	return x + 9;
}
fn f(x : i64) : i64 {
	var a : i64 = x * 2;
	var c : i64 = g(a);
	return a + c * 3 + c;
}
fn main() : i64 {
	if (f(4) != 76) {
		return 1;
	}
	return 0;
}
