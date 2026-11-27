module "vec";

export class Vec {
	x : i64;
	y : i64;

	init (self, x : i64, y : i64) { self.x = x; self.y = y; }

	fn suma(self, o : Vec) : Vec { return Vec(self.x + o.x, self.y + o.y); }

	fn dot(self, o : Vec) : i64 { return self.x * o.x + self.y * o.y; }

	fn origen() : Vec { return Vec(0, 0); }

	op + (self, o : Vec) : Vec { return Vec(self.x + o.x, self.y + o.y); }

	op - (self) : Vec { return Vec(-self.x, -self.y); }

	op == (self, o : Vec) : i64 { return self.x == o.x && self.y == o.y; }

	op [] (self, i : i64) : ptr i64 {
		if (i == 0) return $self.x;
		return $self.y;
	}

	destroy (self) { self.x = 0; self.y = 0; }
}
