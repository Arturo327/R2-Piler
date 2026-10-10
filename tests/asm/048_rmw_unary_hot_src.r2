// OPT: RMW unary (`notq` in memory) on the hot path. `s = s + 1` is an
// `addq $1, mem` and each `s = ~s` a `notq mem`; the double negation is
// the identity, so s grows by 1 per iter: 10000000 after 10M iters.
// grep loop: `addq $1,` and `notq` on the slot, no loads around them.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 10000000; i = i + 1) {
		s = s + 1;
		s = ~s;
		s = ~s;
	}
	if (s == 10000000) {
		return 0;
	}
	return 1;
}
