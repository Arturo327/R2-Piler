class C {
	v : i64;
	init (self, v : i64) { self.v = v; }
	fn inc(self) { self.v = self.v + 1; return; }
	fn add(self, n : i64) { self.v = self.v + n; }
	priv fn twice(self) { self.v = self.v * 2; }
	fn six(self) { self.twice(); self.v = self.v + 3; }
	fn get(self) : i64 { return self.v; }
}

pub fn main() : i64 {
	var c : C = C(1);
	c.inc().add(2).inc().six();
	return c.get();
}
