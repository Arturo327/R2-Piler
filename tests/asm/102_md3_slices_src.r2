arr gm : i32[2][2][3] = {{{1, 2, 3}, {4, 5, 6}}, {{7, 8, 9}, {10, 11, 12}}};

fn sum(v : i32[]) : i64 {
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < v.len) {
		s += v[i] as i64;
		i++;
	}
	return s;
}

fn row2(v : i32[][]) : i64 {
	return sum(v[0]) * 100 + sum(v[1]) * 10 + v[0].len;
}

fn cube(v : i32[][][]) : i64 {
	return row2(v[0]) * 10000 + row2(v[1]) * 1000 + v[0][0].len;
}

fn deep(w : i32[][][][]) : i64 {
	return w.len * 100 + w[0].len * 10 + w[0][0].len;
}

fn main() : i64 {
	arr m : i32[2][2][3] = {{{1, 2, 3}, {4, 5, 6}}, {{7, 8, 9}, {10, 11, 12}}};
	arr q : i32[1][2][3][4] = {{{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}},
		{{13, 14, 15, 16}, {17, 18, 19, 20}, {21, 22, 23, 24}}}};
	var a : i64 = m.len;
	var b : i64 = m[1].len;
	var c : i64 = m[1][0].len;
	var z : i64 = sizeof(m) + sizeof(m[0]) + sizeof(m[0][0]);
	var x : i64 = m[0][1][2] + m[1][1][0] + gm[1][0][1];
	var t : i64 = cube(m) + cube(gm);
	var r : i64 = row2(m[1]) + sum(m[1][0]);
	var d : i64 = deep(q) + q[0][1][2][3] + sizeof(q);
	var k : i64 = 0;
	var acc : i64 = 0;

	if (a != 2 || b != 2 || c != 3 || z != 84) { return 1; }
	if (x != 6 + 10 + 8) { return 2; }
	if (t != 10263003 * 2) { return 3; }
	if (r != 2733 + 24) { return 4; }
	if (d != 123 + 24 + 96) { return 5; }

	m[0][0][0] = 100;
	m[1][1][2] += 5;
	var lhs : i64 = sum(m[0][0]);
	if (lhs != 105 || m[1][1][2] != 17) { return 6; }

	var p : ptr i32 = m;
	@p = 42;
	if (m[0][0][0] != 42) { return 7; }

	while (k < 1000) {
		acc += m[k % 2][0][0] as i64;
		k++;
	}
	if (acc != (42 + 7) * 500) { return 8; }
	return 0;
}