class Bad {
	x : i64;

	init (self) { self.x = 0; }

	op = (self, k : i64) : void { self.x = k; }
}

class NoTruth {
	x : i64;

	init (self) { self.x = 0; }
}

class Cmp {
	x : i64;

	init (self) { self.x = 0; }

	op != (self, b : Cmp) : i64 { return b.x != 0; }
	op - (self, b : Cmp) : Cmp { return b; }
}

class Priv {
	x : i64;

	init (self) { self.x = 0; }

	priv op + (self, b : Priv) : Priv { return b; }
	priv op = (self, b : Priv) : void { self.x = b.x; }
}

pub fn main() : i64
{
	var n : NoTruth = NoTruth();
	var c : Cmp = Cmp();
	var a : i64 = 0;

	if (n) { a = 1; }
	if (!n) { a = 2; }
	if (c) { a = 3; }
	var e : i64 = 5 - c;
	var f : i64 = 5 + c;
	var p : Priv = Priv();
	var q : Priv = Priv();
	var r : Priv = Priv();
	a += r.x;
	p + q;
	r = q;
	return a + e + f + r.x;
}
