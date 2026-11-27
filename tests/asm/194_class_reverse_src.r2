class P {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op + (self, k : i64) : P { return P(self.v + k); }
	op * (self, k : i64) : P { return P(self.v * k); }
	op == (self, k : i64) : i64 { return self.v == k; }
	op != (self, k : i64) : i64 { return self.v != k; }
}

pub fn main() : i64
{
	var p : P = P(10);
	var a : P = 5 + p;
	var b : P = 3 * p;
	var c : i64 = (10 == p) + (11 != p);
	return a.v + b.v + c;
}
