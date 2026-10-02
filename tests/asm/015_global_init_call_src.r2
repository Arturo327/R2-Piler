fn get() : i64 {
	return 42;
}
var a : i64 = get();
var b : i64 = 1;
fn main() : i64 {
	if (a == 42) {
		if (b == 1) {
			return 0;
		}
		return 2;
	}
	return 1;
}
