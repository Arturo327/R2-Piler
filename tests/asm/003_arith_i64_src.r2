fn main() : i64 {
	var a : i64 = 100;
	var b : i64 = 7;
	var c : i64 = a + b * 2 - a / b;
	if (c == 100) {
		return 0;
	}
	return 1;
}
