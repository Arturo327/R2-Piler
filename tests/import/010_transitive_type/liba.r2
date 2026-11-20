pub struct Pair { a : i64; b : i64; }

pub fn make (a : i64, b : i64) : Pair
{
	var p : Pair = {.a = a, .b = b};
	return p;
}
