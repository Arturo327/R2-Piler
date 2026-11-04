extern fn strlen(s : i8[4]) : u64;

arr g : i32[3] = {7, 8, 9};

fn sum3(x : i32[3]) : i64 {
	return (x[0] as i64) + (x[1] as i64) + (x[2] as i64) + x.len;
}

fn bump(x : i32[4]) : void {
	var i : i64 = 0;

	while (i < x.len) {
		x[i] += 1;
		i++;
	}
}

fn relay(x : i32[3]) : i64 {
	return sum3(x);
}

fn total(v : i32[]) : i64 {
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < v.len) {
		s += (v[i] as i64);
		i++;
	}
	return s;
}

fn via_slice(x : i32[3]) : i64 {
	return total(x);
}

fn via_ptr(x : i32[3]) : i64 {
	var p : ptr i32 = x;

	return p[1] as i64;
}

fn many(a : i32[3], b : i32[3], c : i32[3], d : i32[3], e : i32[3],
		f : i32[3], h : i32[3], n : i64) : i64 {
	return (a[0] as i64) + (b[0] as i64) + (c[0] as i64) + (d[0] as i64)
		+ (e[0] as i64) + (f[0] as i64) + (h[0] as i64) + n;
}

fn main() : i64 {
	arr a : i32[3] = {1, 2, 3};
	arr b : i32[3] = {4, 5, 6};
	arr c : i32[4] = {0, 0, 0, 0};
	arr txt : i8[4] = "ok";
	var i : i64 = 0;
	var acc : i64 = 0;
	var dim : i64 = a.len;
	var bytes : i64 = sizeof(a);
	var elem : i64 = sizeof(a[0]);

	if (sum3(a) != 9) { return 1; }
	if (sum3(g) != 27) { return 2; }
	if (relay(b) != 18) { return 3; }
	if (via_slice(a) != 6) { return 4; }
	if (via_ptr(g) != 8) { return 5; }
	if (dim != 3) { return 6; }
	if (bytes != 12) { return 7; }
	if (elem != 4) { return 8; }

	bump(c);
	if (c[0] != 1) { return 9; }
	if (c[3] != 1) { return 10; }
	if (c[1] != 1 || c[2] != 1) { return 11; }
	if (many(a, a, b, b, a, a, b, 100) != 116) { return 12; }
	if (many(g, a, b, b, a, a, b, 0) != 22) { return 13; }

	while (i < 1000) {
		acc += sum3(a) + (b[0] as i64);
		i++;
	}
	if (acc != 13000) { return 14; }
	if (strlen(txt) != 2) { return 15; }
	return 0;
}
