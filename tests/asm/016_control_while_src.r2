fn main() : i64 {
	var s : i64 = 0;
	var i : i64 = 1;
	while (i <= 10) {
		s = s + i;
		i = i + 1;
	}
	if (s == 55) {
		return 0;
	}
	return 1;
}
