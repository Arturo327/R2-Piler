struct P {
	x: i64;
	y: i64;
}

fn a(x : &i64) {
	x = 1;
}

fn b(x : &i64) {
	x += 1;
}

fn c(x : &i64) : ptr i64 {
	return $x;
}

fn w(p : &P) {
	p.x = 1;
}

fn r(p : &P) : i64 {
	return p.y;
}

fn main() : i64 {
	var n : i64 = 0;
	a(n);
	b(n);
	c(n);
	var p : P = {.x = 0, .y = 0};
	w(p);
	return r(p) + n;
}
