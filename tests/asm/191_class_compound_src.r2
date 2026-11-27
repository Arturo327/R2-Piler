class V {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op + (self, b : V) : V { return V(self.v + b.v); }
	op - (self, b : V) : V { return V(self.v - b.v); }
	op += (self, b : V) : void { self.v = self.v + b.v + 100; }
}

class W {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op + (self, k : i64) : W { return W(self.v + k); }
	op * (self, k : i64) : W { return W(self.v * k); }
}

pub fn main() : i64
{
	var a : V = V(5);
	var b : V = V(2);
	var w : W = W(3);

	a += b;
	a -= b;
	var sum : i64 = a.v + w.v;
	w *= 4;
	w += 3;
	return sum + w.v;
}
