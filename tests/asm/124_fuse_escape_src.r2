struct P { x : i64; y : i64; }

fn touch (p : ptr P) : i64 { return p.x; }

fn main () : i64
{
	var p : P = {.x = 3, .y = 4};
	var a : i64 = p.x + p.y;
	return a + touch($p);
}
