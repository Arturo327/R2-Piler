fn add (a : i64, b : i64 = 1, c : i64 = 2) : i64 {
	return a + b + c;
}

fn main () : i64 {
	return add(1) + add(1, 2) + add(1, c = 3) + add(a = 4, b = 5);
}
