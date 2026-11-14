struct P { x : i64; y : i64; }
enum E : u8 { A = 1, B = 2 }

fn pair() : i64, u32 { return 3, 4; }

fn main() : i64 {
	var a = 33;
	var b = pair();
	var s = "hola";
	var c = a + b;
	var e = {.x = 5, .y = 6} as P;
	var u = 5u;
	var ch = 'a';
	var en = E.B;
	var p = $a;
	var sl = 1 << 2;
	static var st = 9;
	var x, y = pair();
	for (var i = 0; i < 1; i++) c += i;
	return c + e.x + e.y + st + (u as i64) + (ch as i64) + (en as i64)
			+ @p + x + y + sl + (s[0] as i64);
}