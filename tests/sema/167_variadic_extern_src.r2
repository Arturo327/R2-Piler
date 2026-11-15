// Only extern declarations may be variadic; the declared parameters are still
// checked and extra arguments are passed as they are.
extern fn printf (fmt : ptr i8, ...) : i32;

pub fn main () : i64 {
	printf("hola\n");
	printf("%d %d\n", 1, 2);
	return 0;
}
