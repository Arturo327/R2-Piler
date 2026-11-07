fn by_index() : i64 {
	arr a : i64[4] = {3, 4, 5, 6};
	arr k : i64[1] = {0};
	var s : i64 = 0;

	for (k[0] : a) {
		s = s + a[k[0]];
	}
	return s;
}

fn by_deref() : i64 {
	arr a : i64[3] = {7, 8, 9};
	arr v : i64[2] = {0, 0};
	var p : ptr i64 = v;
	var s : i64 = 0;

	for (@(p + 1) : a) {
		s = s + a[@(p + 1)];
	}
	return s + v[1];
}

fn main() : i64 {
	if (by_index() != 18) { return 1; }
	if (by_deref() != 27) { return 2; }
	return 0;
}
