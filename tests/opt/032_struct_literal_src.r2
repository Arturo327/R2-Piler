struct P {
	x : i32;
	y : i32;
}

struct Q {
	p : P;
	n : i32;
}

fn sum (v : P) : i64 {
	return v.x as i64 + v.y as i64;
}

fn make () : P {
	return {.x = 10, .y = 20} as P;
}

fn pair () : P, i64 {
	return {.x = 30, .y = 40} as P, 7;
}

fn main () : i64 {
	var a : P = {.x = 1};
	var b : P = {.y = 2} as P;
	var c : P = make();
	var d : P, e : i64 = pair();
	var f : Q = {.p = {.x = 3} as P, .n = 4};
	var g : i64 = sum({.x = 5, .y = 6} as P);
	var h : ptr P = $a;

	a = {.x = 7, .y = 8} as P;
	@h = {.y = 9} as P;

	return a.x as i64 + b.y as i64 + c.x as i64 + d.y as i64 + e
			+ f.n as i64 + g;
}
