import "stdlib";

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { }
}

pub fn main() : i64 {
	var c : C = C(1);
	drop c;
	reset c(2);
	var p : ptr C = new C(3);
	drop p;
	delete p;
	return c.x;
}