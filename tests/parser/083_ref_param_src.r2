struct P {
	x: i64;
	y: i64;
}

fn inc(x : &i64) {
	x += 1;
}

fn advance(p : &ptr i64) {
	p = p + 1;
}

fn field(p : &P) : i64 {
	return p.x;
}

fn main() : i64 {
	var n : i64 = 0;
	inc(n);
	var q : ptr i64 = $n;
	advance(q);
	return field({.x = 0, .y = 0});
}
