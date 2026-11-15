fn take(s : char[]) : i64 { return s.len; }

pub fn main() : i64 {
	var s : char[] = "Hello";
	var n : i64 = s.len;
	s = "hi";
	return n + take("abc") + s.len;
}