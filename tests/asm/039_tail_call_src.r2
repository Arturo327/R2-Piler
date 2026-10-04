// OPT: tail calls (leave+jmp vs call+ret) + deep recursion without stack growth.
// `count`/`tick` end in `return f(...)` with <=6 args and same ret type, so the
// backend emits `leave; jmp __r2_f` (no new frame per step). 1000000 nested
// steps run in constant stack; without the opt this gives SIGSEGV (8MB stack).
// `wrap6` covers the argc==6 boundary (still tail). `011` stays non-tail by design.
// grep: `jmp __r2_count` and `jmp __r2_tick` present, no `call __r2_count` in a loop.
fn count(n : i64, acc : i64) : i64 {
	if (n <= 0) {
		return acc;
	}
	return count(n - 1, acc + 1);
}
fn tick(n : i64) {
	if (n <= 0) {
		return;
	}
	tick(n - 1);
}
fn add6(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64) : i64 {
	return a + b + c + d + e + f;
}
fn wrap6(n : i64) : i64 {
	if (n <= 0) {
		return 0;
	}
	return add6(1, 1, 1, 1, 1, n);
}
fn main() : i64 {
	if (count(1000000, 0) != 1000000) {
		return 1;
	}
	tick(500000);
	if (wrap6(7) != 12) {
		return 2;
	}
	return 0;
}
