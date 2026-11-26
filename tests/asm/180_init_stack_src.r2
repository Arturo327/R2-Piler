class N {
	me : ptr N;
	x : i64;
	init (self, x : i64) { self.me = self; self.x = x; }
	fn get(self) : i64 { return self.x; }
}

struct Box { n : N; tag : i64; }

var g : N = N(10);

fn make(n : i64) : N { return N(n); }

pub fn main() : i64 {
	var a : N = N(1);
	static var s : N = N(2);
	arr arrs : N[2] = {N(3), N(4)};
	var box : Box = {.n = N(5), .tag = 6};

	if (a.me != $a || a.get() != 1) return 1;
	if (g.me != $g || g.get() != 10) return 2;
	if (s.me != $s || s.get() != 2) return 3;
	if (arrs[0].me != $arrs[0] || arrs[1].get() != 4) return 4;
	if (box.n.me != $box.n || box.n.get() != 5) return 5;

	var m : N = make(7);

	if (m.get() != 7) return 6;
	return a.get() + g.get() + s.get() + arrs[1].get() + box.n.get() + m.get() - 29;
}
