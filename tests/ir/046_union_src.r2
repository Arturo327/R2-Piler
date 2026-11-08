union Number {
	i: i64;
	parts: i32[2];
}

fn main() : i64 {
	var n : Number = {.i = 0};

	n.parts[0] = 1;
	n.parts[1] = 2;
	return n.i + (n.parts[1] as i64);
}
