// OPT: bitwise identities + zero shifts folded on the hot path.
// `i & -1`, `| 0`, `^ 0`, `<< 0`, `>> 0` all simplify to MOVE at O1, so the
// loop keeps only `s = s + i`; O0 emits and/or/xor/shl/shr per iteration.
// s = sum 0..7999999 = 31999996000000.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		var a : i64 = i & -1;
		var b : i64 = a | 0;
		var c : i64 = b ^ 0;
		var d : i64 = c << 0;
		var e : i64 = d >> 0;
		s = s + e;
	}
	if (s == 31999996000000) {
		return 0;
	}
	return 1;
}
