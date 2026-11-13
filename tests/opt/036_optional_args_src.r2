var g : i64 = 3;

fn mix (a : i64, b : i64 = 4, c : i64 = 8) : i64 {
	return a * b + c;
}

fn main () : i64 {
	var x : i64 = mix(g);
	var y : i64 = mix(g, c = 2);
	return x + y;
}