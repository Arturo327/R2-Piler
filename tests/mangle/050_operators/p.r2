class C {
	x : i64;
	init (self) { self.x = 0; }
	op + (self, b : C) : C { return b; }
	op - (self) : C { return @self; }
	op - (self, b : C) : C { return b; }
	op * (self, b : C) : C { return b; }
	op / (self, b : C) : C { return b; }
	op % (self, b : C) : C { return b; }
	op == (self, _b : C) : i64 { return 0; }
	op != (self, _b : C) : i64 { return 0; }
	op < (self, _b : C) : i64 { return 0; }
	op <= (self, _b : C) : i64 { return 0; }
	op > (self, _b : C) : i64 { return 0; }
	op >= (self, _b : C) : i64 { return 0; }
	op & (self, b : C) : C { return b; }
	op | (self, b : C) : C { return b; }
	op ^ (self, b : C) : C { return b; }
	op << (self, _k : i64) : C { return @self; }
	op >> (self, _k : i64) : C { return @self; }
	op ! (self) : i64 { return 0; }
	op ~ (self) : C { return @self; }
	op [] (self, i : i64) : ptr i64 { return $self.x + i; }
}

pub fn main() : i64 {
	var _c : C = C();
	return 0;
}
