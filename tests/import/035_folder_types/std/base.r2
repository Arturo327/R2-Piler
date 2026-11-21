pub struct Point { x : i64; y : i64; }

pub fn make (x : i64, y : i64) : Point
{
	var p : Point = {.x = x, .y = y};
	return p;
}
