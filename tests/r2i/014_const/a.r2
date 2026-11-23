pub struct S {
	f : const i64;
	items : const i32[2];
	p : ptr const i64;
	q : const ptr i64;
}

pub var g : const i64 = 3;
pub var cp : const ptr i64 = NULL;

pub fn take (_x : const i32, q : ptr const i64) : ptr const i64
{
	return q;
}

pub fn main () : i64
{
	return 0;
}
