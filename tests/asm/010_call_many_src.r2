fn add7(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64) : i64 {
	return a + b + c + d + e + f + g;
}
fn main() : i64 {
	var r : i64 = add7(1, 2, 3, 4, 5, 6, 7);
	if (r == 28) {
		return 0;
	}
	return 1;
}
