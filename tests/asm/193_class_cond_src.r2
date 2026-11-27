class Ne {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op != (self, z : i64) : i64 { return self.v != z; }
}

class Eq {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op == (self, z : i64) : i64 { return self.v == z; }
}

class Not {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op ! (self) : i64 { return self.v == 0; }
}

pub fn main() : i64
{
	var n0 : Ne = Ne(0);
	var n1 : Ne = Ne(1);
	var e0 : Eq = Eq(0);
	var e5 : Eq = Eq(5);
	var q0 : Not = Not(0);
	var q7 : Not = Not(7);
	var r : i64 = 0;

	if (n1) r += 1;
	if (!n0) r += 2;
	if (!e0) r += 4;
	if (e5) r += 8;
	if (q7) r += 16;
	if (!q0) r += 32;
	if (n1 && q7) r += 64;
	if (n0 || e5) r += 128;
	while (n1) { r += 0; break; }
	while (n0) { r += 1000; }
	var t : i64 = e5 ? 0 : 100;
	return r + t;
}
