fn add (a : i64, b : i64 = 1, c : i64 = 2) : i64 {
	return a + b + c;
}

fn main () : i64 {
	var x : i64 = add(1);
	var y : i64 = add(1, c = 3);
	return x + y;
}