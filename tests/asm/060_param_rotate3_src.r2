// OPT: three-way PARAM/ARG rotation. rot(a, b, c) calls pick(b, c, a); pick
// contains a CALL, so neither rot nor pick is inlined at -O2/-O3.
// rot(1, 2, 3) -> pick(2, 3, 1) = (2 * 7) * 100 + 3 * 10 + 1 = 1431.
fn mix(x : i64) : i64 {
	return x * 7;
}
fn pick(a : i64, b : i64, c : i64) : i64 {
	return mix(a) * 100 + b * 10 + c;
}
fn rot(a : i64, b : i64, c : i64) : i64 {
	return pick(b, c, a);
}
fn main() : i64 {
	if (rot(1, 2, 3) == 1431) {
		return 0;
	}
	return 1;
}
