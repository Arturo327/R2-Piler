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
	return {.x = 1, .y = 2} as P;
}

fn pair () : P, i64 {
	return {.x = 3, .y = 4} as P, 5;
}

fn main () : i64 {
	var a : P = {.x = 6};
	var b : P = {.y = 7} as P;
	var c : P = {} as P;
	var d : P = make();
	var e : P, f : i64 = pair();
	var g : Q = {.p = {.x = 8} as P, .n = 9};
	var h : i64 = sum({.x = 10, .y = 11} as P);
	var i : i64 = ({.x = 12} as P).x as i64;
	arr j : P[2] = {{.x = 13} as P, {.y = 14}};
	var k : ptr P = $a;

	@k = {.x = 15, .y = 16} as P;
	a = {.x = 17} as P;

	return a.x as i64 + b.y as i64 + c.x as i64 + d.x as i64
			+ e.y as i64 + f + g.n as i64 + h + i + j[1].y as i64;
}
