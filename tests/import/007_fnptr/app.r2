import "lib";

pub fn main () : i64
{
	var f : ptr fn(i64) : i64 = lib::twice;
	return f(21);
}
