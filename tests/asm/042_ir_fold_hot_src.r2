// OPT: IR fold/simplify (+0, *1, /1, %1) on the hot path.
// Every iteration does `t = i + 0` (-> MOVE), `u = t * 1` (-> MOVE),
// `w = u / 1` (-> MOVE) and `z = w % 1` (-> const 0), so O1 keeps only
// `s = s + i` while O0 emits add/imul plus idivq for /1 and %1.
// s = sum 0..7999999 = 31999996000000.
// grep O0: `idivq` present in loop; O1: no `idivq`/`imulq` in loop.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		var t : i64 = i + 0;
		var u : i64 = t * 1;
		var w : i64 = u / 1;
		var z : i64 = w % 1;
		s = s + u + z;
	}
	if (s == 31999996000000) {
		return 0;
	}
	return 1;
}
