module "dep";

pub class D {
	v : i64;

	init (self, v : i64) { self.v = v; }

	fn get(self) : i64 { return self.v; }

	destroy (self) { }
}
