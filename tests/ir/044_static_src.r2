var g : i64 = 7;

fn counter() : i64 {
	static var x : i64 = 0;

	x = x + 1;
	return x;
}

fn zero() : i64 {
	static var z : i64;

	return z;
}

fn main() : i64 {
	return counter() + zero();
}