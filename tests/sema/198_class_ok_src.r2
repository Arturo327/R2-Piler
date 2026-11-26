class Caja {
	edad : i64;
	init (self, edad : i64) { self.edad = edad; }
	priv fn valida(self) : i64 { return self.edad; }
	fn cambia(self, v : i64) : void { self.edad = valida(self) + v; }
	fn doble(x : i64) : i64 { return x * 2; }
}

pub fn main() : i64 {
	var c : Caja = Caja(30);
	c.cambia(4);
	return c.edad + Caja::doble(2);
}
