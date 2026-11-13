fn f() : i64 {
	var c : i64 = 0;

	while (c < 3) {
		c++;
	} else {
		return 1;
	}
}

fn main() : i64 {
	return f() - 1;
}