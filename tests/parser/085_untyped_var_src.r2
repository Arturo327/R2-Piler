struct P { x : i64; }

fn f() : i64, u32 { return 1, 2; }

pub fn main() : i64 {
	var a = 33;
	var s = "hola";
	var e = {.x = 5} as P;
	static var st = 7;
	var x, y = f();
	for (var i = 0; i < 3; i++) { a += i; }
	return a + x + y + st + (s[0] as i64) + e.x;
}