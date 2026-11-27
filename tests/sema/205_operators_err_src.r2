class A {
	x : i64;
	init (self) { self.x = 0; }
}

class B {
	x : i64;
	init (self) { self.x = 0; }
	op + (self, b : B) : B { return b; }
	op + (self, b : B) : B { return b; }
	op ! (self, b : B) : i64 { return 0; }
	op [] (self) : ptr i64 { return $self.x; }
	op == (self, b : B) : i8 { return 0; }
	op - (self, b : B) : B { return b; }
	op * (self, k : i64 = 1) : B { return self; }
	op / (self, rest : ...B) : B { return B(); }
	op % (a : B, b : B) : B { return b; }
	op << (self, k : i64) : i64, i64 { return k, k; }
}

class D {
	x : i64;
	init (self) { self.x = 0; }
	op [] (self, p : ptr i8) : ptr i64 { return $self.x; }
}

class M {
	x : i64;
	init (self) { self.x = 0; }
	op [] (self, i : i64, j : i64) : ptr i64 { return $self.x + i + j; }
}

class F {
	f : ptr fn() : i64;
	init (self, f : ptr fn() : i64) { self.f = f; }
	op [] (self, _i : i64) : ptr fn() : i64 { return self.f; }
}

fn zero () : i64 { return 0; }

pub fn main() : i64 {
	var a : A = A();
	var b : B = B();
	var d : D = D();
	var m : M = M();
	var f : F = F(zero);
	var _c : B = a + b;
	var _u : B = -b;
	var _n : i64 = !b;
	var _i : i64 = b[0];
	var _r : i64 = 5 + b;
	var _q : i64 = b == b;
	b.add(b);
	b += b;
	b++;
	var _j : i64 = d[0];
	var _k : i64 = m[0];
	var _l : i64 = m[0, 0, 0];
	f[0] = f[0];
	$f[0];
	arr vals : i64[3];
	var _z : i64 = vals[0, 1];
	return 0;
}