struct P {
	x : i32;
	y : i32;
}

struct Q {
	p : P;
	n : i32;
}

fn sum (v : P) : i64 {
	return (v.x as i64) + (v.y as i64);
}

fn make () : P {
	return {.x = 10, .y = 20} as P;
}

fn pair () : P, i64 {
	return {.x = 30, .y = 40} as P, 7;
}

fn main () : i64 {
	var p : P = {.x = 1};
	var q : P = {.y = 2} as P;
	var e : P = {} as P;
	var m : P = make();
	var o : Q = {.p = {.x = 7} as P, .n = 8};
	var n : i64 = sum({.x = 5, .y = 6} as P);
	var a : P, b : i64 = pair();
	var pp : ptr P = $p;

	if (p.x != 1) return 1;
	if (p.y != 0) return 2;
	if (q.x != 0) return 3;
	if (q.y != 2) return 4;
	if (e.x != 0) return 5;
	if (e.y != 0) return 6;
	if (m.x != 10) return 7;
	if (m.y != 20) return 8;
	if (o.p.x != 7) return 9;
	if (o.p.y != 0) return 10;
	if (o.n != 8) return 11;
	if (n != 11) return 12;
	if (a.x != 30) return 13;
	if (a.y != 40) return 14;
	if (b != 7) return 15;

	@pp = {.x = 100, .y = 200} as P;
	if (p.x != 100) return 16;
	if (p.y != 200) return 17;

	arr s : P[2] = {{.x = 9} as P, {.y = 10}};

	if (s[0].x != 9) return 18;
	if (s[0].y != 0) return 19;
	if (s[1].x != 0) return 20;
	if (s[1].y != 10) return 21;

	return 0;
}
