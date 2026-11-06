var g : i64 = 3;

fn counter() : i64 {
	static var x : i64 = 0;
	static var y : i64 = 5;

	x = x + 1;
	return x + y;
}

fn ptrstatic() : i64 {
	static var p : ptr i64 = $g;

	return @p;
}

fn main() : i64 {
	return 0;
}