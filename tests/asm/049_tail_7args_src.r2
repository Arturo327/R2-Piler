// OPT: tail-call boundary. `wrap7` ends in `return add7(...)` with 7 args,
// but tail calls only cover argc <= 6, so this must stay a real `call`
// (7th arg on the stack) instead of `leave; jmp`.
// wrap7(7) = 1 + 1 + 1 + 1 + 1 + 1 + 7 = 13.
// grep: `call __r2_add7` present, no `jmp __r2_add7`.
fn add7(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64) : i64 {
	return a + b + c + d + e + f + g;
}
fn wrap7(n : i64) : i64 {
	if (n <= 0) {
		return 0;
	}
	return add7(1, 1, 1, 1, 1, 1, n);
}
fn main() : i64 {
	if (wrap7(7) == 13) {
		return 0;
	}
	return 1;
}
