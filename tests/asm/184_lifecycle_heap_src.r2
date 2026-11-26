from "stdlib" import malloc, free;

var count : i64 = 0;

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { count += self.x; }
}

pub fn main() : i64 {
	var p : ptr C = new C(2);
	delete p;
	var q : ptr C = new(malloc(sizeof(C))) C(3);
	delete(free) q;
	var r : ptr C = new C(4);
	drop r;
	return count;
}