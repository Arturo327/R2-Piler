pub struct Inner {
	a : i64;
	b : u8 : 3;
	c : i5;
}

pub struct Outer {
	in : Inner;
	raw : Inner[2];
	p : ptr Inner;
	sl : Inner[];
	fp : fn(i64) : i64;
	union {
		x : u32;
		y : i32;
	}
}

pub union U {
	big : i64;
	small : i32;
}

pub packed struct P {
	x : u8;
	y : i32;
}

pub enum Signed : i8 {
	Min = -1,
	Max = 3,
}

pub type Alias = Outer;
pub newtype Handle = u64;

pub fn take (_o : Outer, _u : U, _p : P, _s : Signed, _a : Alias, _h : Handle) : void
{
}

pub fn main () : i64
{
	return 0;
}