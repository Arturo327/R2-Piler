export fn _r2_foo () : i64 { return 0; }
export var __r2_bar : i64 = 1;

pub fn main () : i64 {
	export fn nested () : i64 { return 1; }
	export var v : i64 = 2;
	pub fn g () : i64 { return 3; }
	return 0;
}
