class P {
	x : i64;
	init (self, x : i64) { self.x = x; }
	fn add (self, b : P) : i64 { return self.x + b.x + 100; }
	op + (self, b : P) : P { return P(self.x + b.x); }
	op [] (self, i : i64) : ptr i64 { return $self.x + i; }
}

pub fn main() : i64 {
	arr a : P[3];
	a[0] = P(4);
	a[1] = P(5);
	a[2] = P(6);
	var p : ptr P = a;
	var q : ptr P = p + 1;
	var sum : P = a[1] + a[2];
	return (@q).x + p[1].x + sum.x + a[1].add(a[2]);
}
