struct P {
	x: i64;
	y: i64;
}

fn movep(p : &P, dx : i64) {
	p.x += dx;
}

fn inc(v : &i64) {
	v += 1;
}

fn copy(dst : &P, src : &P) {
	dst = src;
}

fn main() : i64 {
	var a : P = {.x = 1, .y = 2};
	movep(a, 4);
	if (a.x != 5) return 1;
	inc(a.x);
	if (a.x != 6) return 2;
	arr v : i64[2] = {10, 20};
	inc(v[0]);
	if (v[0] != 11) return 3;
	var q : ptr i64 = $a.x;
	inc(@q);
	if (a.x != 7) return 4;
	var b : P = {.x = 0, .y = 0};
	copy(b, a);
	if (b.x != 7) return 5;
	if (b.y != 2) return 6;
	return 0;
}
