fn get() : i64 {
	return 7;
}
var init : i64 = get();
fn main() : i64 {
	if (init == 7) {
		return 0;
	}
	return 1;
}
