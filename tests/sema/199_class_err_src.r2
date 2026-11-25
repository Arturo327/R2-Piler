class Caja {
	valor : i64;
	fn valor(self) : i64 { return 1; }
	priv fn oculto(self) : i64 { return self.valor; }
	fn malo(self, self) : i64 { return 1; }
}

class Vacia {
	fn solo(x : i64) : i64 { return x; }
}

pub class Publica {
	valor : i64;
}

pub fn main() : i64 {
	var c : Caja = {.valor = 1};
	var f : ptr fn(ptr Caja) : i64 = Caja::oculto;
	return c.oculto() + c.no_existe();
}
