class Fns {
	f : ptr fn(i64) : i64;
	init (self, f : ptr fn(i64) : i64) { self.f = f; }
	op [] (self, _i : i64) : ptr fn(i64) : i64 { return self.f; }
}

class P {
	x : i64;
	init (self, x : i64) { self.x = x; }
	op [] (self, i : i64) : ptr void { return $self.x + i; }
}

fn add1 (x : i64) : i64 { return x + 1; }

pub fn main() : i64 {
	var t : Fns = Fns(add1);
	var p : P = P(7);
	var q : ptr i64 = p[0] as ptr i64;
	return t[0](35) + @q;
}
