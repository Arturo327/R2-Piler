class V {
	op : i64;
	x : i64;
	fn op(self) { }
	init (self) { }
	op + (self, b : V) : V { return b; }
	op - (self) : V { return @self; }
	op [] (self, i : i64, j : i64) : ptr i64 { return $self.x; }
}

pub fn main() : i64 {
	var v : V = V();
	var w : V = v + v;
	var x : i64 = w[1, 2];
	return 0;
}