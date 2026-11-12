struct P {
	x : i32;
	y : i32;
}

fn main () : i64 {
	var a : P = {1, 2};
	var b : i64 = {};
	var c : P = {.x = 1, .y = 2};
	var d : i32[2] = {1, 2};

	return a.x + b + c.y + d[0] as i64;
}
