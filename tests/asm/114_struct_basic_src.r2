struct Point {
	x: i32;
	y: i32;
}

struct Box {
	tag: i8;
	p: Point;
	size: u16;
}

var base : Point = {.x = 100, .y = 200};

fn main() : i64 {
	var b : Box = {.tag = 1, .p = {.x = 10, .y = 20}, .size = 4};
	arr ps : Point[3] = {{.x = 1, .y = 2}, {.x = 3, .y = 4}, {.x = 5, .y = 6}};
	var acc : i64 = 0;

	for (var i : i64 = 0; i < 3; i++) {
		acc += (ps[i].x + ps[i].y) as i64;
	}

	b.p.y = b.p.y + (acc as i32);
	var sz : i64 = sizeof(Box);

	if (sz != 16) return 1;
	if (b.tag != 1) return 2;
	if (b.p.x != 10) return 3;
	if (b.p.y != 41) return 4;
	if (base.x + base.y != 300) return 5;

	return 0;
}
