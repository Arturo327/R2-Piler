module "ops";

pub class Acc {
	x : i64;

	init (self, x : i64) { self.x = x; }

	op = (self, b : Acc) : void { self.x = b.x; }

	op += (self, b : Acc) : void { self.x += b.x; }

	op ++ (self) : void { self.x++; }

	op + (self, k : i64) : Acc { return Acc(self.x + k); }

	op != (self, z : i64) : i64 { return self.x != z; }

	priv op - (self, b : Acc) : Acc { return Acc(self.x - b.x); }

	fn diff(self, b : Acc) : Acc { return @self - b; }
}
