import lib;

pub fn main () : i64
{
	var p : ptr i64 = $lib::g;

	lib::g = lib::g + 1;
	lib::table[2] = 30;
	return lib::g + lib::total() + (p != NULL ? 0 : 100);
}
