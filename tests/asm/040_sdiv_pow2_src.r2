// OPT: signed DIV/MOD by power of two (bias+shr/and vs idivq) on the hot path.
// Every iteration does `i / 8` (-> bias sequence + sarq $3) and `i % 8`
// (-> bias + andq $7 + sub). A single `idivq` is ~20-40 cycles; the whole loop
// is ~10x slower without it. Mirrors 031 (unsigned) for the signed path.
// grep: `sarq $3` present, `idivq`/`divq` absent in loop.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 2000000; i = i + 1) {
		var q : i64 = i / 8;
		var r : i64 = i % 8;
		s = s + q + r;
	}
	if (s == 250006000000) {
		return 0;
	}
	return 1;
}
