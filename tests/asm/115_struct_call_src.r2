struct Point {
	x: i32;
	y: i32;
}

var g : Point = {.x = 7, .y = 8};

fn bump(p : Point) : i32 {
	p.x = p.x + 100;
	p.y = p.y + 200;
	return p.x + p.y;
}

fn make(x : i32, y : i32) : Point {
	var p : Point = {.x = x, .y = y};

	return p;
}

fn sum(p : Point) : i32 {
	return p.x + p.y;
}

fn main() : i64 {
	var a : Point = {.x = 1, .y = 2};
	var r : i32 = bump(a);

	if (a.x != 1) return 1;
	if (a.y != 2) return 2;

	var m : Point = make(30, 40);
	var s : i32 = sum(m);

	if (s != 70) return 3;

	m = make(g.x, g.y);
	if (m.x != 7) return 4;
	if (m.y != 8) return 5;
	if (r + s + m.x + m.y != 388) return 6;

	return 0;
}
