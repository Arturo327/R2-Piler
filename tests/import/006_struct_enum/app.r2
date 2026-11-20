import lib;

pub fn main () : i64
{
	var p : lib::Pair = lib::make(3, 4);
	var e : lib::E = lib::E.Two;
	return lib::sum(p) + (e as i64);
}
