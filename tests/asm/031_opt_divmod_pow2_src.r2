// OPT: unsigned DIV/MOD by power of two (shr/and vs divq) on the hot path.
// Every iteration does `i / 8` (-> shrq $3) and `i % 8` (-> andq $7).
// A single `divq` is ~20-40 cycles; the whole loop is ~10x slower without it.
// grep: `shrq $3` and `andq $7` present, no `divq`/`idivq` in loop.
fn main() : i64 {
	var s : u64 = 0;
	for (var i : u64 = 0; i < 2000000; i = i + 1) {
		var q : u64 = i / 8;
		var r : u64 = i % 8;
		s = s + q + r;
	}
	if (s == 250006000000) {
		return 0;
	}
	return 1;
}
