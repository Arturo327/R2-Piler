class C {
	x : i64;
	init (self) { self.x = 0; }
	op = (self, b : C) : void { self.x = b.x; }
	op += (self, b : C) : void { self.x += b.x; }
	op >>= (self, k : i64) : void { self.x >>= k; }
	op ++ (self) : void { self.x++; }
	op -- (self) : void { self.x--; }
	priv op - (self) : C { return self; }
}

pub fn main() : i64 {
	return 0;
}
