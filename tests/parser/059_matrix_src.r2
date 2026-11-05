arr m : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
arr names : i8[2][4] = {"ab", "cd"};

fn rows(v : i32[][]) : i64 {
	return (v[0][1] as i64) + v.len + v[1].len;
}

fn main() : i64 {
	return rows(m) + (m[1][2] as i64);
}
