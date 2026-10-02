var g : i64 = 0;
fn set(v : i64) {
	g = v;
}
fn main() : i64 {
	set(42);
	if (g == 42) {
		return 0;
	}
	return 1;
}
