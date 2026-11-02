extern fn puts (s : ptr char) : i32;
extern var optind : i32;

fn hello () : i32 {
	return puts("hi");
}

fn main () : i64 {
	var a : i32 = hello();
	var b : i32 = puts("bye");
	return (a + b + optind) as i64;
}