// OPT: unused PARAMs mixed with used ones. keep() ignores a and c; both must
// be skipped by the PARAM group (no move, no stack load) while b and d still
// arrive correctly. keep contains a CALL, so it stays out of the inliner.
// call_keep(1, 2, 3, 4) -> keep(4, 3, 2, 1) = id(3) + 1 = 4.
fn id(x : i64) : i64 {
	return x;
}
fn keep(a : i64, b : i64, c : i64, d : i64) : i64 {
	return id(b) + d;
}
fn call_keep(a : i64, b : i64, c : i64, d : i64) : i64 {
	return keep(d, c, b, a);
}
fn main() : i64 {
	if (call_keep(1, 2, 3, 4) == 4) {
		return 0;
	}
	return 1;
}
