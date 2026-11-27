module "lib";

pub class Acc {
	x : i64;

	init (self, x : i64) { self.x = x; }

	op = (self, b : Acc) : void { self.x = b.x + 1; }

	op += (self, b : Acc) : void { self.x = self.x + b.x + 10; }

	op ++ (self) : void { self.x = self.x + 100; }

	op + (self, k : i64) : Acc { return Acc(self.x + k); }

	op != (self, z : i64) : i64 { return self.x != z; }
}
