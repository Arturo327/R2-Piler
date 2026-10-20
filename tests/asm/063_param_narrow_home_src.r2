// OPT: narrow PARAM whose home is a physical register. The loop gives `a`
// enough weight to get a register home at -O1 and above, so the PARAM group
// emits a real register-to-register move and then re-extends the narrow
// image in place. f contains a CALL, so it is never inlined.
// f(-3, 5) = -3 * 5 = -15.
fn id(x : i64) : i64 {
	return x;
}
fn f(a : i8, n : i64) : i64 {
	var s : i64 = 0;
	var i : i64 = 0;
	while (i < n) {
		s = s + (a as i64);
		i = i + 1;
	}
	return id(s);
}
fn main() : i64 {
	if (f((0 - 3) as i8, 5) == (0 - 15)) {
		return 0;
	}
	return 1;
}
