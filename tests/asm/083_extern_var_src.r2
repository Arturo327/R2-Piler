extern var optind : i32;

var op : ptr i32 = $optind;

fn main () : i64 {
	if (@op != optind) { return 1; }
	optind = 7;
	if (@op != 7) { return 2; }
	optind = optind + 1;
	@op = @op + 1;
	return optind as i64;
}