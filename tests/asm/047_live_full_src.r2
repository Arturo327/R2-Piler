// OPT: FULL-only live DCE across branches. `t = a * b` dies on both sides:
// `x = t` is overwritten by `x = 1` / `x = 2` before any use, so liveness
// kills the MOVE and then the MUL. O1 (plain DCE) keeps the `imulq` per
// call; O2 removes it. 2000000 calls make O1 > O2 in Ir.
// s = 1000000 * 2 + 1000000 * 1 = 3000000.
fn f(a : i64, b : i64, c : i64) : i64 {
	var t : i64 = a * b;
	var x : i64 = t;
	if (c != 0) {
		x = 1;
	} else {
		x = 2;
	}
	return x;
}
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 2000000; i = i + 1) {
		s = s + f(i, 7, i & 1);
	}
	if (s == 3000000) {
		return 0;
	}
	return 1;
}
