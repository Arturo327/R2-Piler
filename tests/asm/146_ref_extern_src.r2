extern fn memset(s : &i8, c : i32, n : u64) : ptr void;

fn main() : i64 {
	var b : i8 = 0;
	memset(b, 65, 1);
	if (b != 65) return 1;
	return 0;
}
