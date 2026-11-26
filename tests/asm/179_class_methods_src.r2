class Caja {
	x : i64;

	init (self, x : i64) { self.x = x; }

	priv fn base(self) : i64 { return self.x; }

	fn add(self, v : i64) : void { self.x = base(self) + v; }

	fn get(self) : i64 { return self.x; }

	fn twice(a : i64) : i64 { return a * 2; }
}

fn make(n : i64) : Caja { return Caja(n); }

pub fn main() : i64
{
	var c : Caja = Caja(1);
	c.add(4);
	var p : ptr Caja = $c;
	p.add(2);
	var t : i64 = make(3).get();
	return c.get() + t + p.twice(5) + Caja::twice(1);
}
