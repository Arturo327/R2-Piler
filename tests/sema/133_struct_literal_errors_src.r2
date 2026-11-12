struct P {
	x : i32;
	y : i32;
}

fn naked () : P {
	return {.x = 1};
}

fn main () : i64 {
	var a : P = {.x = 1};
	var b : P = {.y = 2} as P;
	var g : P = {3, 4} as P;
	var c : i64 = {.x = 5} as i64;
	var d : i64 = {6, 7} as i64;
	var e : P = {.x = 8} as ptr P;
	var f : P = {.x = 9} as P;

	return a.x as i64 + b.y as i64 + g.x as i64 + c + d + e.x as i64 + f.x as i64;
}
