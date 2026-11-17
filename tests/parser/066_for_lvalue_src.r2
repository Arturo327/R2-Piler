pub fn main() : i64 {
	arr a : i64[4];
	var p : ptr i64 = a;

	for (@p : a) { }
	for (@(p + 1) : a) { }
	for (a[3] : a) { }
	return 0;
}
