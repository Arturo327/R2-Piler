struct P {
	x: i64;
	y: i64;
}

fn inc(x : &i64, d : i64) {
	x += d;
}

fn twice(x : &i64) {
	inc(x, 1);
}

fn get(x : &i64) : i64 {
	return x;
}

fn addr(x : &i64) : ptr i64 {
	return $x;
}

fn field(p : &P) : i64 {
	return p.y;
}

fn set(p : &P, v : i64) {
	p.y = v;
}

fn main() : i64 {
	var n : i64 = 10;
	inc(n, 5);
	twice(n);
	var m : i64 = get(n);
	var q : ptr i64 = addr(n);
	if (q != $n) return 1;
	var a : P = {.x = 1, .y = 2};
	set(a, 7);
	return m + field(a);
}
