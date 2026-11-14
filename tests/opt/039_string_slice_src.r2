fn main() : i64 {
	var s : char[] = "Hello, world";
	var i : i64 = 0;
	var acc : i64 = 0;
	while (i < s.len) {
		acc += s[i] as i64;
		i += 1;
	}
	return acc;
}