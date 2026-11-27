module "acc";

export class Acc {
	x : i64;

	init (self, x : i64) { self.x = x; }

	op = (self, b : Acc) : void { self.x = b.x; }

	op += (self, b : Acc) : void { self.x += b.x + 10; }

	op ++ (self) : void { self.x += 100; }

	op != (self, z : i64) : i64 { return self.x != z; }
}
