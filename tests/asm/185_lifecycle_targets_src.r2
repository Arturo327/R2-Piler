import "stdlib";

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { }
	fn renew(self) { reset self(9); }
}

struct S { c : C; }

fn make() : ptr C { return new C(1); }

pub fn main() : i64 {
	var c : C = C(1);
	arr a : C[2] = {C(2), C(3)};
	var s : S = {.c = C(4)};
	var p : ptr C = new C(5);

	reset a[1](6);
	reset s.c(7);
	reset @p(8);
	c.renew();

	drop a[0];
	drop s.c;
	drop @p;
	drop make();
	var px : i64 = p.x;
	delete p;
	return a[1].x * 1000 + s.c.x * 100 + px * 10 + c.x;
}