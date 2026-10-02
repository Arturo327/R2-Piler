fn fact(n : i64) : i64 {
	if (n <= 1) {
		return 1;
	}
	return n * fact(n - 1);
}
fn main() : i64 {
	if (fact(5) == 120) {
		return 0;
	}
	return 1;
}
