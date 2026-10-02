fn main() : i64 {
	var a : u64 = 18446744073709551615;
	var b : u64 = 2;
	var c : u64 = (a / b) as u64;
	if (c == 9223372036854775807) {
		return 0;
	}
	return 1;
}
