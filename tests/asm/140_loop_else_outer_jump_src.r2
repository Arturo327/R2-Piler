fn main() : i64 {
	var s : i64 = 0;
	var flag : i64 = 0;

	for (var i : i64 = 0; i < 3; i++) {
		while (flag) {
			s += 100;
		} else {
			if (i == 1) break;
			s += 1;
		}
	}
	return s - 1;
}