fn imp(x : i64) : i64 {
	return x + 1;
}

fn pure(c : i64, a : i64, b : i64) : i64 {
	return c ? a : b;
}

fn impure(c : i64, a : i64, b : i64) : i64 {
	return c ? imp(a) : imp(b);
}

fn main() : i64 {
	return 0;
}