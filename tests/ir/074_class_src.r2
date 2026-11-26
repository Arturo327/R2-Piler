class Caja {
	x : i64;
	init (self, x : i64) { self.x = x; }
	priv fn valida(self) : i64 { return self.x; }
	fn cambia(self, v : i64) : void { self.x = valida(self) + v; }
	fn suma(self) : i64 { return valida(self) + self.x; }
	fn doble(x : i64) : i64 { return x * 2; }
}

pub fn main() : i64 {
	var c : Caja = Caja(1);
	c.cambia(4);
	return c.suma() + Caja::doble(3);
}
