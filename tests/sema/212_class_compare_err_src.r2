class A { x : i64; }
class B { x : i64; }

pub fn main() : i64 {
	var a : A;
	var b : B;
	var r : i64 = 0;
	r = a == b;
	r = a == 1;
	r = a < b;
	return r;
}
