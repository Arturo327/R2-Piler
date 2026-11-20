pub struct Pair { a : i64; b : i32; }

pub enum E : u32 { One = 1, Two = 2, }

pub fn sum (p : Pair) : i64 { return p.a + p.b; }

pub fn make (a : i64, b : i64) : Pair
{
	var p : Pair = {.a = a, .b = b as i32};
	return p;
}
