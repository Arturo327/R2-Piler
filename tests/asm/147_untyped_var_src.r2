struct P { x : i64; y : i64; }

fn pair() : i64, u32 { return 3, 4; }

fn bump() : i64 {
	static var n = 0;
	n += 1;
	return n;
}

fn main() : i64 {
	var a = 33;
	var s = "hola";
	var e = {.x = 5, .y = 6} as P;
	var u = 5u;
	var ch = 'a';
	var x, y = pair();
	static var st = 4;
	var total = a + e.x + e.y + (u as i64) + (ch as i64) + x + y + st;
	var i = 0;
	for (var k = 0; k < 3; k++) { total += k; i += 1; }
	if (s[0] != 'h') return 1;
	if (i != 3) return 2;
	total += bump() + bump();
	return total;
}