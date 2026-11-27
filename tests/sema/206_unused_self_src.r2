class Caja {
	x : i64;

	init (self, v : i64) { self.x = v; }

	fn no_usa(self) : i64 { return 7; }

	op + (self, o : Caja) : Caja { return Caja(o.x); }

	destroy (self) { }

	fn usa(self) : i64 { return self.x; }
}

pub fn main() : i64
{
	var a : Caja = Caja(3);
	var b : Caja = a + a;
	var r : i64 = a.no_usa() + a.usa() + b.usa();

	drop a;
	drop b;
	return r;
}