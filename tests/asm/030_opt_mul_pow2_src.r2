// OPT: MUL strength reduction and MUL+ADD lea fusion on the hot path.
// Every iteration adds `i * 8` and `i * 5` into s; the pairs fuse to
// `leaq (s,i,8)` and `leaq (s,i,4); addq i, s`, with no imul/shl in the loop.
// grep: a `leaq` with scale 8 and an `addq` of the loop index, no `imulq`.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 1000; i = i + 1) {
		s = s + i * 8 + i * 5;
	}
	if (s == 6493500) {
		return 0;
	}
	return 1;
}
