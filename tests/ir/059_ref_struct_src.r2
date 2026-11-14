struct P {
	x: i64;
	y: i64;
}

fn movep(p : &P, dx : i64) {
	p.x += dx;
}

fn copy(dst : &P, src : &P) {
	dst = src;
}

fn gety(p : &P) : i64 {
	return p.y;
}

fn main() : i64 {
	var a : P = {.x = 1, .y = 2};
	var b : P = {.x = 0, .y = 0};
	movep(a, 3);
	copy(b, a);
	return gety(b);
}
