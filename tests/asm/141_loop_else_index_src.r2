fn main() : i64 {
	arr m : i64[2][2] = {{1, 2}, {3, 4}};
	var e : i64 = 0;

	for (var i, j : m) {
		if (j == 1) break;
	} else {
		e += 1;
	}
	return e - 1;
}