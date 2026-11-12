struct P {
	x : i32;
}

fn use (p : P) : i64 {
	return p.x as i64;
}

fn main () : i64 {
	var n : i64 = 0;

	if (n == 0) {
		n = 1;
	}
	while (n < 2) {
		n += 1;
	}
	{
		n += use({.x = 3} as P);
	}
	if (use({.x = 4} as P) == 4) {
		n += 1;
	}
	return n;
}
