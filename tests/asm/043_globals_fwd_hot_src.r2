// OPT: global load forwarding (LD_GLOBAL -> MOVE) on the hot path.
// `b = g` right after `a = g` in the same block forwards to a MOVE, so O1
// does one RIP-relative load per iter instead of two. The const init also
// folds to .data via static_init.
// s = sum (12345 + 12345 + i), i in 0..999 = 25189500.
// grep O1 loop: single `g(%rip)` load per iter.
var g : i64 = 12345;
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 1000; i = i + 1) {
		var a : i64 = g;
		var b : i64 = g;
		s = s + a + b + i;
	}
	if (s == 25189500) {
		return 0;
	}
	return 1;
}
