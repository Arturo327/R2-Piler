import "stdlib";

class NoDestroy {
	x : i64;
	init (self, x : i64) { self.x = x; }
}

class NoInit {
	x : i64;
	destroy (self) { }
}

class BadSelf {
	x : i64;
	destroy (self) { }
	fn bad(self) { drop self; }
}

class BadDecl {
	x : i64;
	fn destroy(self) { }
}

class Extra {
	x : i64;
	destroy (self, n : i64) { }
}

class Dup {
	x : i64;
	destroy (self) { }
	destroy (self) { }
}

class C {
	x : i64;
	init (self, x : i64) { self.x = x; }
	destroy (self) { }
}

fn take(p : ptr C) {
	delete p;
	drop p;
}

fn byval(c : C) {
	drop c;
}

fn byref(c : &C) {
	drop c;
}

fn byref_ptr(p : &ptr C) {
	delete p;
}

pub fn main() : i64 {
	var d : NoDestroy = NoDestroy(1);
	drop d;
	reset d(2);
	var e : NoInit = NoInit();
	reset e();
	var c : C = C(1);
	c.destroy();
	C::destroy($c);
	var cp : ptr const C = $c;
	drop cp;
	reset cp(1);
	var _d : ptr fn(ptr C) : void = C::destroy;
	var _i : ptr fn(ptr C) : void = C::init;
	var n : i64 = 1;
	drop n;
	reset n(1);
	drop C(1);
	return 0;
}