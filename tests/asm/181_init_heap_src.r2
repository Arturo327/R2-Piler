from "stdlib" import malloc, free;

class P {
	x : i64;
	init (self, x : i64) { self.x = x; }
	fn get(self) : i64 { return self.x; }
}

type A = P;

pub fn main() : i64 {
	var p : ptr P = new P(5);
	if (p.get() != 5) return 1;
	var q : ptr P = new P;
	q.x = 6;
	if (q.get() != 6) return 2;
	var r : ptr A = new A(7);
	if (r.get() != 7) return 3;
	var s : ptr P = new(malloc(sizeof(P))) P(8);
	if (s.get() != 8) return 4;
	var t : P = A(9);
	if (t.get() != 9) return 5;

	delete p;
	delete q;
	delete r;
	delete(free) s;
	return 0;
}
