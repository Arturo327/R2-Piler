struct Interno {
	v : i64;
}

export struct Grande {
	a : i64;
	b : i64;
	c : i64;
}

arr datos : i64[2];

export class Exp {
	campo : i64;

	init (self) { self.campo = 0; }

	fn malo(self, x : Interno) : i64 { return 0; }

	fn template(self) : i64 { return 1; }

	fn variadico(self, xs : ...i64) : i64 { return 0; }

	fn multi(self) : i64, i32 { return 1, 2; }

	fn grande(self, g : Grande) : i64 { return g.a; }

	fn matriz(self, m : i64[][]) : i64 { return 0; }

	fn vector(self) : i64[2] { return datos; }

	fn vista(self) : i64[] { return datos; }
}

pub fn main() : i64 { return 0; }
