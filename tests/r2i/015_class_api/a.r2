module "caja";

pub class Caja {
	x : i64;
	y : i64;

	init (self, x : i64, y : i64 = 0) { self.x = x; self.y = y; }

	priv fn oculto(self) : i64 { return 0; }

	fn get(self) : i64 { return self.x + self.y + self.oculto(); }

	fn origen() : Caja { return Caja(0); }

	op + (self, o : Caja) : Caja { return Caja(self.x + o.x); }

	op - (self) : Caja { return Caja(-self.x); }

	op [] (self, i : i64) : ptr i64 {
		if (i == 0) return $self.x;
		return $self.y;
	}

	destroy (self) { }
}

export class Punto {
	x : i64;

	init (self, x : i64) { self.x = x; }

	fn cero() : Punto { return Punto(0); }
}
