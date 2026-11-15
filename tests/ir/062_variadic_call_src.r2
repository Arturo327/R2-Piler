// Extra arguments of a variadic extern call become plain ARG slots after the
// declared ones (the seventh one goes on the stack at codegen time).
extern fn printf (fmt : ptr i8, ...) : i32;

pub fn main () : i64 {
	var r : i32 = printf("%d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7);
	return r as i64;
}
