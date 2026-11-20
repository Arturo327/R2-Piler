import lib;

pub fn f (cb : ptr fn(i64) : i64 = lib::id, v : ptr i64 = $lib::g) : i64
{
	return cb(@v);
}
