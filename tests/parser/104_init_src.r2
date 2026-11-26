class Caja {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { }
	priv fn p(self) : i64 { return self.x; }
}

extern fn malloc (n : u64) : ptr void;

pub fn main() : i64 {
	var a : Caja = Caja(1);
	var b : ptr Caja = new Caja(2);
	var c : ptr Caja = new(malloc(8)) Caja(3);
	var d : ptr Caja = new Caja;
	return a.x;
}
