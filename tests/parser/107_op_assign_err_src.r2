class C {
	x : i64;
	init (self) { self.x = 0; }
	op = (self) : C { return self; }
	op += (self, b : C) : C { return b; }
}

pub fn main() : i64 {
	return 0;
}
