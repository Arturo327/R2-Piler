// OPT: 7-argument call whose sources are rotated across the register/stack
// boundary: g(v7, v1, v2, v3, v4, v5, v6). The stack store for ARG 6 must be
// emitted before the register permutation, and scratch loads for spilled
// sources must not clobber a pending argument. spread/g contain CALLs, so
// they are not inlined at -O2/-O3.
// spread(1..7) -> g(7, 1, 2, 3, 4, 5, 6) = (7 + 1) + 2 + 3 + 4 + 5 + 6 = 28.
fn add(a : i64, b : i64) : i64 {
	return a + b;
}
fn g(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, h : i64) : i64 {
	return add(a, b) + c + d + e + f + h;
}
fn spread(v1 : i64, v2 : i64, v3 : i64, v4 : i64, v5 : i64, v6 : i64, v7 : i64) : i64 {
	return g(v7, v1, v2, v3, v4, v5, v6);
}
fn main() : i64 {
	if (spread(1, 2, 3, 4, 5, 6, 7) == 28) {
		return 0;
	}
	return 1;
}
