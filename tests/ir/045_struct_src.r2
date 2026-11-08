struct Point {
	x: i32;
	y: i32;
}

struct Line {
	a: Point;
	b: Point;
}

var g : Point = {.x = 5, .y = 6};

fn make(v : i32) : Point {
	var p : Point = {.x = v, .y = v + 1};

	return p;
}

fn take(p : Point) : i32 {
	p.x = p.x + 1;
	return p.x + p.y;
}

fn main() : i64 {
	var a : Point = {.x = 1, .y = 2};
	var l : Line = {.a = {.x = 3, .y = 4}, .b = a};
	var m : Point = make(l.a.x);
	var r : i64 = (take(m) as i64) + (g.x as i64);

	l.b = m;
	return r + (a.y as i64);
}
