module "geom";

pub class Punto {
	x : i64;
	y : i64;

	init (self, x : i64, y : i64 = 0) { self.x = x; self.y = y; }

	fn suma(self, o : Punto) : Punto { return Punto(self.x + o.x, self.y + o.y); }

	fn get(self, i : i64) : i64 { if (i == 0) return self.x; return self.y; }

	fn origen() : Punto { return Punto(0); }

	op + (self, o : Punto) : Punto { return Punto(self.x + o.x, self.y + o.y); }

	op [] (self, i : i64) : ptr i64 {
		if (i == 0) return $self.x;
		return $self.y;
	}

	destroy (self) { self.x = 0; self.y = 0; }
}
