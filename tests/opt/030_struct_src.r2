struct Point {
	x: i32;
	y: i32;
}

fn add(a : Point, b : Point) : Point {
	var r : Point = {.x = a.x + b.x, .y = a.y + b.y};

	return r;
}

fn twice(v : i32) : i32 {
	return v * 2;
}

fn main() : i64 {
	var a : Point = {.x = 1, .y = 2};
	var b : Point = {.x = 3, .y = 4};
	var c : Point = add(a, b);
	var d : i32 = twice(c.x) + twice(c.y);

	return d as i64;
}
