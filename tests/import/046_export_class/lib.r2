module "exp";

export class E {
	v : i64;

	init (self, v : i64) { self.v = v; }

	fn get(self) : i64 { return self.v; }

	fn doble() : E { return E(2); }

	destroy () { }
}
