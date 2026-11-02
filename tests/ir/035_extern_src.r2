extern fn puts (s : ptr char) : i32;
extern var optind : i32;

var op : ptr i32 = $optind;

fn main () : i64 {
	var r : i32 = puts("hi");
	optind = optind + r;
	return @op as i64;
}