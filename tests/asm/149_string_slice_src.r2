pub fn main() : i64 {
	var s : char[] = "Hello, world";
	var n : i64 = s.len;
	if (s[0] != 'H') return 1;
	if (s[11] != 'd') return 2;
	s = "hi";
	if (s.len != 2) return 4;
	var t : char[] = "abc";
	var p : char[] = s;
	return n + t.len + s.len + p.len;
}