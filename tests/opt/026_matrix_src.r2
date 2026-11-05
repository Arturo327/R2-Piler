arr g : i32[2][2] = {{1, 2}, {3, 4}};

fn at(m : i32[][], i : i64, j : i64) : i64 {
	return m[i][j] as i64;
}

fn main() : i64 {
	arr a : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
	var n : i64 = a.len + a[1].len;
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < n) {
		s += at(a, 1, i % 3);
		i++;
	}
	return s + at(g, 0, 1);
}
