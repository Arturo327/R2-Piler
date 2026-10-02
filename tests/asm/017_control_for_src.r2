fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 10; i = i + 1) {
		s = s + i;
	}
	if (s == 45) {
		return 0;
	}
	return 1;
}
