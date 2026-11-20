pub struct Pair { a : i64; b : i64; }

pub enum E : u32 { One = 1, Two = 2, }

pub type Id = i64;

pub newtype Handle = u64;

pub fn make (a : i64, b : i64) : Pair
{
	var p : Pair = {.a = a, .b = b};
	return p;
}
