class V {
	x : i64;
	y : i64;
	init (self, x : i64, y : i64) { self.x = x; self.y = y; }
	fn add(self, b : V) : i64 { return self.x + b.x; }
	op + (self, b : V) : V { return V(self.x + b.x, self.y + b.y); }
	op - (self) : V { return V(-self.x, -self.y); }
	op * (self, k : i64) : V { return V(self.x * k, self.y * k); }
	op == (self, b : V) : i64 { return self.x == b.x && self.y == b.y; }
	op ! (self) : i64 { return self.x == 0 && self.y == 0; }
	op & (self, b : V) : V { return V(self.x & b.x, self.y & b.y); }
	op << (self, k : i64) : V { return V(self.x << k, self.y << k); }
	op ~ (self) : V { return V(~self.x, ~self.y); }
	op [] (self, i : i64) : ptr i64 { return $self.x + i; }
}

class M {
	n : i64;
	init (self, n : i64) { self.n = n; }
	op [] (self, i : i64, j : i64) : ptr i64 {
		return $self.n + i * self.n + j;
	}
}

class F {
	f : ptr fn(i64) : i64;
	init (self, f : ptr fn(i64) : i64) { self.f = f; }
	op [] (self, _i : i64) : ptr fn(i64) : i64 { return self.f; }
}

class R {
	x : i64;
	init (self, x : i64) { self.x = x; }
	op [] (self, i : i64) : ptr void { return $self.x + i; }
}

type W = V;

fn inc (x : i64) : i64 { return x + 1; }

pub fn main() : i64 {
	var a : V = V(2, 3);
	var b : V = V(4, 5);
	var c : V = a + b;
	var d : V = -a;
	var e : V = a * 2;
	var q : i64 = a == b;
	var n : i64 = !a;
	var m : V = a & b;
	var s : V = a << 2;
	var t : V = ~a;
	var i : i64 = c[0];
	var j : i64 = M(3)[1, 2];
	var ff : F = F(inc);
	var k : i64 = ff[0](1);
	var r : R = R(7);
	var p : ptr i64 = r[0] as ptr i64;
	var l : i64 = @p;
	var w : W = a;
	var z : i64 = w.add(b);
	return i + j + k + l + q + n + z + d.x + e.x + m.x + s.x + t.x;
}