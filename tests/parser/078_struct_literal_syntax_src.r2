struct P {
	x : i32;
	y : i32;
}

struct Q {
	p : P;
	n : i32;
}

fn sum (v : P) : i64 {
	return v.x as i64;
}

fn make () : P {
	return {.x = 1, .y = 2} as P;
}

fn main () : i64 {
	var a : P = {.x = 1};
	var b : P = {.y = 2} as P;
	var c : P = {} as P;
	var d : Q = {.p = {.x = 3} as P, .n = 4};
	var e : i64 = sum({.x = 5} as P);
	var f : i64 = ({.x = 6} as P).x as i64;
	arr g : P[2] = {{.x = 7} as P, {.y = 8}};

	a = {.x = 9, .y = 10} as P;
	return a.x as i64 + b.x as i64 + c.y as i64 + d.n as i64 + e + f
			+ g[0].x as i64;
}
