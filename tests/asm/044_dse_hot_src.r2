// OPT: DSE of an overwritten local (kills `t = i * 3`) + DCE of the dead
// MOVE chain on the hot path. `t` and the first `u` never reach a use, so
// O1 emits no multiply while O0 computes `i * 3` via `lea` every iteration.
// s = sum (i + 1), i in 0..7999999 = 32000004000000.
// grep: no `lea` in the O1 .s at all.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		var t : i64 = i * 3;
		var u : i64 = t;
		u = i + 1;
		s = s + u;
	}
	if (s == 32000004000000) {
		return 0;
	}
	return 1;
}
