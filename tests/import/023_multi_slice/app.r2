import lib;

pub fn main () : i64
{
	var p : lib::Pair = lib::pair(1, 2);
	var a : i64, b : i64 = lib::pair2(3, 4);
	arr items : i64[3] = {1, 2, 3};
	return p.a + p.b + a + b + lib::sum(items)
			+ sizeof(lib::Pair) + offsetof(lib::Pair, b);
}
