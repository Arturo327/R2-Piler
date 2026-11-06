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

fn other() : i64 {
	static var x : i64 = 100;

	x = x + 1;
	return x;
}

fn arrsum() : i64 {
	static arr a : i64[3] = {1, 2, 3};

	a[0] = a[0] + 1;
	return a[0] + a[1] + a[2];
}

fn ptrstatic() : i64 {
	static var p : ptr i64 = $g;

	return @p;
}

fn blockstat() : i64 {
	var r : i64;

	{
		static var y : i64 = 10;
		y = y + 1;
		r = y;
	}
	return r;
}

fn main() : i64 {
	return counter() + zero() + other() + arrsum() + ptrstatic() + blockstat();
}