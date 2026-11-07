var g : i64 = f();

fn f() : i64 {
	static var s : i64 = 5;
	static var t : i64 = 6;

	return s + t;
}

fn main() : i64 {
	if (g != 11) { return 1; }
	return 0;
}
