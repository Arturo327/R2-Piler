import lib;

pub fn main () : i64
{
	var h : lib::H = {.a = 1, .b = 2} as lib::H;
	var u : lib::U = {.x = 5} as lib::U;
	var b : lib::B = {.a = 3, .b = 4} as lib::B;
	return lib::sum(h) + u.y + b.a + b.b;
}
