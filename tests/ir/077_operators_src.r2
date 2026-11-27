class V {
	x : i64;
	y : i64;
	init (self, x : i64, y : i64) { self.x = x; self.y = y; }
	op + (self, b : V) : V { return V(self.x + b.x, self.y + b.y); }
	op - (self) : V { return V(-self.x, -self.y); }
	op == (self, b : V) : i64 { return self.x == b.x && self.y == b.y; }
	op [] (self, i : i64) : ptr i64 { return $self.x + i; }
}

class F {
	f : ptr fn(i64) : i64;
	init (self, f : ptr fn(i64) : i64) { self.f = f; }
	op [] (self, _i : i64) : ptr fn(i64) : i64 { return self.f; }
}

fn inc (x : i64) : i64 { return x + 1; }

pub fn main() : i64 {
	var a : V = V(1, 2);
	var b : V = V(3, 4);
	var c : V = a + b;
	var d : V = -a;
	var q : i64 = a == b;
	var i : i64 = c[0];
	var f : F = F(inc);
	var k : i64 = f[0](5);
	return i + k + d.x + q;
}
