class D {
	a : i64;
	b : i64;
	init (self, a : i64 = 1, b : i64 = 2) { self.a = a; self.b = b; }
	fn sum(self) : i64 { return self.a + self.b; }
}

class V {
	n : i64;
	init (self, rest : ...i64) {
		self.n = 0;
		for (var i : rest) { self.n += rest[i]; }
	}
}

class R {
	v : i64;
	init (self, x : &i64) { self.v = x; x += 1; }
}

struct S { d : D; tag : i64; }

pub fn main() : i64 {
	var a : D = D();
	if (a.sum() != 3) return 1;
	var b : D = D(b = 9);
	if (b.sum() != 10) return 2;
	var v : V = V(1, 2, 3);
	if (v.n != 6) return 3;
	arr xs : i64[3] = {4, 5, 6};
	var w : V = V(xs...);
	if (w.n != 15) return 4;
	var n : i64 = 10;
	var r : R = R(n);
	if (n != 11 || r.v != 10) return 5;
	var c : D = (a.a != 0) ? D(5) : D(6);
	if (c.sum() != 7) return 6;
	var s : S = {.d = D(7, 8), .tag = 1};
	if (s.d.sum() + s.tag != 16) return 7;
	arr ds : D[2] = {D(1), D(2)};
	if (ds[1].sum() != 4) return 8;
	return 0;
}
