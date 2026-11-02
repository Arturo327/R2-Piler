extern fn puts (s : ptr char) : i32;
extern fn abs (x : i32) : i32;
extern var optind : i32;

fn main () : i64 {
	var n : i32 = abs(-3);
	puts("hola");
	return optind as i64 + n as i64;
}