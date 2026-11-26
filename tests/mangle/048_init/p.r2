class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
}

pub fn main() : i64 { var c : C = C(1); return c.x; }
