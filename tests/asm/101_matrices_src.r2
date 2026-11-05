arr gm : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

fn total(v : i32[]) : i64 {
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < v.len) {
		s += v[i] as i64;
		i++;
	}
	return s;
}

fn sum(m : i32[][]) : i64 {
	var t : i64 = 0;
	var x : i64 = 0;

	while (x < m.len) {
		t += total(m[x]);
		x++;
	}
	return t;
}

fn bump(m : i32[][]) : void {
	m[0][0] += 1;
}

fn desc(v : i32[]) : i64 {
	return sizeof(v) + v.len;
}

fn desc2(m : i32[][]) : i64 {
	return sizeof(m) + sizeof(m[0]) + m.len + m[0].len;
}

fn main() : i64 {
	arr m : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
	arr b : i32[3][2] = {{1, 2}, {3, 4}, {5, 6}};
	arr s : i8[2][4] = {"ab", "cd"};
	arr gz : i64[3][2] = {{1, 2}, {3, 4}, {5, 6}};
	var n : i64 = m.len;
	var c : i64 = m[0].len;
	var z : i64 = sizeof(m) + sizeof(m[0]) + sizeof(m[0][0]);
	var t : i64 = sum(m);
	var f : i64 = total(m[1]);
	var w : i64 = b[2][1] + b[2].len;
	var k : i64 = 0;
	var acc : i64 = 0;

	if (n != 2 || c != 3 || z != 40) { return 1; }
	var ds : i64 = desc(m[0]) + desc2(m);
	if (ds != 64) { return 12; }
	if (t != 21 || f != 15) { return 2; }
	if (w != 8) { return 3; }
	if (sum(b) != 21 || total(b[1]) != 7) { return 4; }
	if (gm[1][2] != 6 || sum(gm) != 21) { return 5; }
	if (s[0][0] != 97 || s[1][1] != 100 || s[0][3] != 0) { return 6; }

	m[0][1] += 10;
	m[1][2]++;
	var p : ptr i32 = $m[1][1];
	@p = 50;
	if (sum(m) != 1 + 12 + 3 + 4 + 50 + 7) { return 7; }
	if (@p != 50) { return 8; }
	bump(m);
	if (m[0][0] != 2) { return 9; }

	gz[2][1] = 30;
	gz[0][0] = 7;
	if (gz[2][1] != 30 || gz[1][0] != 3) { return 10; }

	while (k < 1000) {
		acc += m[k % 2][0] as i64;
		k++;
	}
	if (acc != 3000) { return 11; }
	return 0;
}
