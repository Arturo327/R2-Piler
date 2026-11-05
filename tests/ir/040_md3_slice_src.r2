fn first(v : i32[]) : i32 {
	return v[0];
}

fn row(v : i32[][]) : i32 {
	return first(v[1]);
}

fn cube(c : i32[][][]) : i64 {
	c[1][0][2] = 7;

	var x : i32 = first(c[0][1]);
	var r : i32 = row(c[0]);
	return c.len + c[1].len + c[1][0].len + (r as i64) + (x as i64);
}

fn main() : i64 {
	arr m : i32[2][2][3] = {{{1, 2, 3}, {4, 5, 6}}, {{7, 8, 9}, {10, 11, 12}}};
	return cube(m) + (m[1][1][2] as i64);
}