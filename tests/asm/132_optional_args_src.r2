enum E : i32 { A = 10, B = 20 }
struct P { x : i64; y : i64; }
var g : i64 = 7;

fn sum (a : i64, b : i64 = 1, c : i64 = 2) : i64 {
	return a + b + c;
}

fn pick (a : i64, b : i64 = 4) : i64 {
	return a * b;
}

fn addr (p : ptr i64 = $g) : i64 {
	return @p;
}

fn st (p : P = { .x = 2, .y = 3 } as P) : i64 {
	return p.x + p.y;
}

fn en (e : E = E.B) : i64 {
	return e as i64;
}

fn main () : i64 {
	var r : i64 = 0;
	r += sum(10);
	r += sum(10, 20);
	r += sum(10, c = 20);
	r += sum(b = 5, a = 10);
	r += pick(3);
	r += pick(3, 5);
	r += addr();
	r += st();
	r += en();
	return r;
}