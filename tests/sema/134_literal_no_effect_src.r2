struct P {
	x : i32;
	y : i32;
}

fn side () : i32 {
	return 1;
}

fn main () : i64 {
	var p : P = {.x = 1};
	var n : i64 = p.x as i64;

	p = {.y = 2} as P;
	({.x = 3} as P);
	({} as P);
	({.x = side()} as P);

	return p.x as i64 + n;
}
