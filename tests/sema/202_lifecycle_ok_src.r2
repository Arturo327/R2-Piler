from "stdlib" import malloc, free;

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { }
	fn renew(self) { reset self(3); }
}

fn by_value(c : C) : i64 {
	reset c(6);
	return c.x;
}

fn by_ptr(p : ptr C) : i64 {
	reset p(7);
	return p.x;
}

pub fn main() : i64 {
	var c : C = C(1);
	drop c;
	reset c(2);
	var p : ptr C = new C(4);
	delete p;
	var q : ptr C = new(malloc(sizeof(C))) C(5);
	delete(free) q;
	var r : ptr C = new C(8);
	var v : i64 = by_value(c) + by_ptr(r);
	delete r;
	return c.x + v;
}