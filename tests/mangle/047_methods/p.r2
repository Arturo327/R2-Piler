class C {
	x : i64;
	fn m(self) : i64 { return self.x; }
	fn n(v : i64) : i64 { return v + 1; }
}

pub fn main() : i64 { var c : C = {.x = 1}; return c.m() + C::n(1); }
