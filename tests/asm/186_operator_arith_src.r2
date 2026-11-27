class N {
	v : i64;
	init (self, v : i64) { self.v = v; }
	op + (self, b : N) : N { return N(self.v + b.v); }
	op - (self) : N { return N(-self.v); }
	op ! (self) : i64 { return self.v == 0; }
	op & (self, b : N) : N { return N(self.v & b.v); }
	op << (self, k : i64) : N { return N(self.v << k); }
	op ~ (self) : N { return N(~self.v); }
	op == (self, b : N) : i64 { return self.v == b.v; }
}

pub fn main() : i64 {
	var a : N = N(3);
	var b : N = N(5);
	var c : N = a + b;
	var d : N = -a;
	var e : N = a & b;
	var f : N = a << 2;
	var g : N = ~a;
	var eq : i64 = a == b;
	return c.v + d.v + e.v + f.v + g.v + !eq;
}
