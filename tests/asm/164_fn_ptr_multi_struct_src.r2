struct P { x : i32; y : i32; }

fn pair (a : i32) : i32, u32
{
	return a + 1, (a * 2) as u32;
}

fn mk (a : i32) : P
{
	return {.x = a, .y = a + 1} as P;
}

pub fn main () : i64
{
	var mp : ptr fn(a : i32) : (i32, u32) = pair;
	var m : ptr fn(a : i32) : P = mk;
	var a, b = mp(5);
	var p : P = m(20);
	return a as i64 + b as i64 + p.x as i64 + p.y as i64;
}
