from "stdlib" import malloc;

class A { x : i64; init (x : i64) { } }
class B { x : i64; init (self, self) { } }
class C { x : i64; init (self) { } init (self) { } }
class D { x : i64; fn init (self) { } }
class E { x : i64; priv fn init (self) { } }
class F { x : i64; init (self) { } fn F (self) { } }
class G { x : i64; init (self) { } }
class H { x : i64; }
class I { init : i64; }
class J { destroy : i64; }
class K { x : i64; init (self) { } }
newtype N = K;

pub fn main() : i64 {
	var g : G = { .x = 1 };
	var h : H = H(1);
	var k : K = K();
	var q : ptr K = new(malloc(8)) K();
	var z : K = K::init($k);

	delete q;
	k.init();
	return 0;
}
