extern fn puts (s : ptr char) : i32;
extern var optind : i32;

fn main () : i64 {
	puts("hola");
	optind = optind + 1;
	return optind as i64;
}