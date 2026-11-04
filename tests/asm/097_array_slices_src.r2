fn total(v : i32[]) : i64 {
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < v.len) {
		s += (v[i] as i64);
		i++;
	}
	return s;
}

fn bump(p : ptr i32) : void {
	p[0] += 1;
	p[1] += 2;
}

fn relay(v : i32[]) : i64 {
	return total(v) + v.len;
}

fn spread(a : i32[], b : i32[], c : i32[], d : i32[], x : i64, y : i64) : i64 {
	return (a.len + b.len + c.len + d.len + x + y
		+ (a[0] as i64) + (b[0] as i64) + (c[0] as i64) + (d[0] as i64));
}

fn main() : i64 {
	arr a : i32[3] = {1, 2, 3};
	arr b : i32[2] = {4, 5};
	arr c : i32[1] = {6};
	arr d : i32[4] = {7, 8, 9, 10};
	var p : ptr i32 = a;
	var k : i64 = 0;
	var acc : i64 = 0;

	p[0] = 10;
	bump(a);
	bump($a[0]);
	if (a[0] != 12 || a[1] != 6) { return 1; }
	if (total(a) != 21) { return 2; }
	if (relay(a) != 24) { return 3; }
	if (spread(a, b, c, d, 100, 200) != 339) { return 4; }
	while (k < 250) {
		acc += relay(a);
		k++;
	}
	if (acc != 6000) { return 5; }
	return 0;
}
