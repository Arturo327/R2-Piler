// OPT: DCE of unused pure defs (uses==0 emits nothing) hot.
// `dead = i * 12345` has zero uses, so its `imulq` is never emitted.
// Making `dead` live (e.g. `s = s + i + (dead - dead)`) forces the `imulq`
// back and the loop goes from ~18ms to ~29ms for 8M iterations.
// grep: no `imulq` in the .s at all.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		var dead : i64 = i * 12345;
		s = s + i;
	}
	if (s == 31999996000000) {
		return 0;
	}
	return 1;
}
