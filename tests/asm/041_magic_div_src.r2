// OPT: DIV/MOD by arbitrary constant via magic multiplication (mulq/imulq +
// shifts vs idiv/divq) on the hot path. Every iteration does signed `i / 7`,
// `i % 7` (-> imulq magic) and unsigned `j / 10`, `j % 10` (-> mulq magic).
// A single `idivq`/`divq` is ~20-40 cycles; the loop is several times slower
// without it. Complements 031/040 (pow2) for the magic path.
// grep: `mulq`/`imulq` with `movabsq $0x...` present, no `idiv`/`divq` in loop.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 1000000; i = i + 1) {
		var a : i64 = i / 7;
		var b : i64 = i % 7;
		s = s + a + b;
	}
	var t : u64 = 0;
	for (var j : u64 = 0; j < 1000000; j = j + 1) {
		var c : u64 = j / 10;
		var d : u64 = j % 10;
		t = t + c + d;
	}
	if (s != 71431071426) {
		return 1;
	}
	if (t != 50004000000) {
		return 2;
	}
	return 0;
}
