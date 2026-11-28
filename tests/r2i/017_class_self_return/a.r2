module "chain";

export class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	fn set(self, v : i64) { self.x = v; }
}
