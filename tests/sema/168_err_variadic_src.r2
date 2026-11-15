// '...' is only valid in extern declarations, and variadic calls still check
// the declared parameters and reject named arguments.
extern fn printf (fmt : ptr i8, ...) : i32;

fn bad (a : i64, ...) : i64 { return a; }

pub fn main () : i64 {
	printf(1);
	printf(fmt = "x", 2);
	return 0;
}
