from "stdlib" import malloc;

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	fn get(self) : i64 { return self.x; }
}

fn make(n : i64) : C { return C(n); }

var g : C = C(1);

pub fn main() : i64 {
	var a : C = C(2);
	var p : ptr C = new C(3);
	var q : ptr C = new(malloc(sizeof(C))) C(4);
	var t : C = (a.x != 0) ? C(5) : C(6);

	return make(7).get() + a.get() + p.get() + q.get() + t.get() + g.get();
}
