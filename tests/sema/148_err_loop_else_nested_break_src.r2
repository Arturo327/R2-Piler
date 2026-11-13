fn f() : i64 {
	while (1) {
		while (1) {
			return 1;
		} else {
			break;
		}
	}
}

fn main() : i64 {
	return f();
}