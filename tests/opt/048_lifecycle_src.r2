class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { }
}

pub fn main() : i64 {
	var c : C = C(1);
	drop c;
	reset c(2);
	drop c;
	return c.x;
}