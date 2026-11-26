var destroys : i64 = 0;
var last : i64 = 0;

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) {
		destroys += 1;
		last = self.x;
	}
}

pub fn main() : i64 {
	var c : C = C(5);
	drop c;
	reset c(7);
	drop c;
	return destroys * 10 + last;
}