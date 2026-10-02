fn get() : i64 {
	return 1;
}
var g : i64 = get();
fn init() : i64 {
	return 9;
}
fn main() : i64 {
	if (g == 1) {
		if (init() == 9) {
			return 0;
		}
		return 2;
	}
	return 1;
}
