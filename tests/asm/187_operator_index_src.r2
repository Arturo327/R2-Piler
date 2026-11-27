extern fn malloc (n : u64) : ptr void;
extern fn free (p : ptr void) : void;

struct S { a : i64; b : i64; }

class H {
	s : S;
	init (self) { self.s.a = 1; self.s.b = 2; }
	op [] (self, i : i64) : ptr S { return $self.s + i; }
}

class Mat {
	data : ptr i64;
	n : i64;
	init (self, n : i64) { self.n = n; self.data = malloc(72); }
	destroy (self) { free(self.data); }
	op [] (self, i : i64, j : i64) : ptr i64 {
		return self.data + i * self.n + j;
	}
}

pub fn main() : i64 {
	var h : H = H();
	h[0].a = 2;
	h[0].b = 40;
	var m : Mat = Mat(3);
	m[1, 2] = 42;
	var x : i64 = m[1, 2];
	drop m;
	return h[0].a + h[0].b + x;
}
