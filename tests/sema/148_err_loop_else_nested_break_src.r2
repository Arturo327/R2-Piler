fn f() : i64 {
	while (1) {
		while (1) {
			return 1;
		} else {
			break;
		}
	}
}

pub fn main() : i64 {
	return f();
}