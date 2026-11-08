struct Bad {
	x: i32;
	x: i8;
}

struct P {
	x: i32;
	y: i32;
}

fn main() : i64 {
	var a : P = {.x = 1, .x = 2, .y = 3};
	var b : P = {.x = 1};
	var c : P = {.x = 1, .y = 2, .z = 3};
	var d : P = {.x = 1, .y = "hola"};
	return (a.x + b.y + c.x) + d.y;
}
