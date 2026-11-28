class P {
	x : i64;
	y : i8;
	init (self, x : i64, y : i8) { self.x = x; self.y = y; }
}

class Q {
	v : i64;
	init (self, v : i64) { self.v = v; }
	op == (self, o : Q) : i64 { return self.v == o.v; }
}

pub fn main() : i64 {
	var a : P = P(3, 4);
	var b : P = P(3, 4);
	var c : P = P(3, 5);
	var q : Q = Q(7);
	var r : i64 = 0;
	if (a == b) r = r + 1;
	if (a != c) r = r + 2;
	if (a == c) r = r + 100;
	if (q != Q(8)) r = r + 4;
	if (q == Q(7)) r = r + 8;
	return r;
}
