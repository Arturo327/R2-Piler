enum E : i32 { A = 1, B = 2 }
struct P { x : i64; y : i64; }
var g : i64 = 7;

fn mix (a : i64, b : i64 = 2, s : ptr i8 = "z", p : ptr i64 = $g, e : E = E.B) : i64 {
	return a + b + e as i64 + @s + @p;
}

fn main () : i64 {
	var x : i64 = mix(1);
	x += mix(1, 2, "a");
	x += mix(1, s = "b", b = 4);
	return x;
}
