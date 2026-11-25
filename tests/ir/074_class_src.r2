class Caja {
	x : i64;
	priv fn valida(self) : i64 { return self.x; }
	fn cambia(self, v : i64) : void { self.x = valida(self) + v; }
	fn suma(self) : i64 { return valida(self) + self.x; }
	fn doble(x : i64) : i64 { return x * 2; }
}

pub fn main() : i64 {
	var c : Caja = {.x = 1};
	c.cambia(4);
	return c.suma() + Caja::doble(3);
}
