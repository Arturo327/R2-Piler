import "lib";

pub fn main() : i64 {
	var b : lib::Box = lib::make(5);
	var p : ptr const i64 = lib::addr();
	var x : lib::CI = 7;
	var w : lib::W = lib::wrap();

	return lib::read(b) + @p + lib::peek($b.v) + lib::take(x) + (w.v as i64);
}
