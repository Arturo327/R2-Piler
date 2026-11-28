class C {
	x : i64;
	y : i8;
	init (self, x : i64, y : i8) { self.x = x; self.y = y; }
}

class D {
	x : i64;
	init (self, x : i64) { self.x = x; }
	op == (self, o : D) : i64 { return self.x == o.x; }
}

class E {
	x : i64;
	init (self, x : i64) { self.x = x; }
	op != (self, o : E) : i64 { return self.x != o.x; }
}

pub fn main() : i64 {
	var a : C = C(1, 2);
	var b : C = C(1, 2);
	var d : D = D(3);
	var e : E = E(3);
	var r : i64 = 0;
	if (a == b) r = r + 1;
	if (a != b) r = r + 100;
	if (d == D(3)) r = r + 10;
	if (e != E(4)) r = r + 1000;
	return r;
}
