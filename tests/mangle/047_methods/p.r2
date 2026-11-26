class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	fn m(self) : i64 { return self.x; }
	fn n(v : i64) : i64 { return v + 1; }
}

pub fn main() : i64 { var c : C = C(1); return c.m() + C::n(1); }
