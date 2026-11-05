arr g : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

fn first(m : i32[2][3]) : i64 {
	return (m[0][1] as i64) + m.len + m[0].len;
}

fn sum(m : i32[][]) : i64 {
	var s : i64 = 0;
	var x : i64 = 0;

	while (x < m.len) {
		var y : i64 = 0;
		while (y < m[x].len) {
			s += m[x][y] as i64;
			y++;
		}
		x++;
	}
	return s;
}

fn main() : i64 {
	arr a : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

	return sum(a) + sum(g) + first(a);
}
