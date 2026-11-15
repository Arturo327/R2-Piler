pub fn main() : i64 {
	var a = 10;
	var b = a + 5;
	var s = "hi";
	var c = s[1] as i64;
	var u = 2u;
	static var st = 3;
	return b + c + (u as i64) + st;
}