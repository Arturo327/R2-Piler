module "big";

pub struct Trio {
	a : i64;
	b : i64;
	c : i64;
}

pub fn suma(t : Trio) : i64 { return t.a + t.b + t.c; }
