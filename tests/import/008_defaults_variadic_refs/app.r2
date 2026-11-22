import "lib";

pub fn main () : i64
{
	var n : i64 = 1;
	lib::bump(n);
	return lib::with_default(1) + lib::sum_all(1, 2, 3) + n;
}
