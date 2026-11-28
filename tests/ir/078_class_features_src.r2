class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	fn set(self, v : i64) { self.x = v; }
}

pub fn main() : i64 {
	var a : C = C(1);
	var b : C = C(1);
	var eq : i64 = a == b;
	a.set(2).set(3);
	return eq + a.x;
}
