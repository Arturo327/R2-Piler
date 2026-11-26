from "stdlib" import malloc, free;

class P {
	x : i64;
	y : i64;
	init (self, x : i64 = 1, y : i64 = 2) { self.x = x; self.y = y; }
	fn sum(self) : i64 { return self.x + self.y; }
}

class V {
	n : i64;
	init (self, rest : ...i64) {
		self.n = 0;
		for (var i : rest) { self.n += rest[i]; }
	}
}

struct S { p : P; z : i64; }

type A = P;

var g : P = P(10, 20);
var gp : ptr P = new P(3, 4);
var gq : ptr P = new(malloc(sizeof(P))) P(5, 6);

pub fn main() : i64 {
	var a : A = A(1);
	var b : P = P(y = 7);
	var c : P = (a.x != 0) ? P(8) : P(9);
	var s : S = {.p = P(2, 3), .z = 4};
	arr pa : P[2] = {P(1), P(2)};
	var q : ptr P = new P();
	var r : ptr P = new(malloc(sizeof(P))) P(1, 2);
	var v : V = V(1, 2, 3);
	static var sp : P = P(11);

	delete gp;
	delete q;
	delete(free) gq;
	delete(free) r;
	if (g.sum() + gp.sum() + gq.sum() + a.sum() + b.sum() + c.sum() != 0) return 1;
	return s.p.sum() + pa[0].sum() + v.n + sp.sum();
}
