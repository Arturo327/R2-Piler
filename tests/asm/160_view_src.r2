arr g : i32[6] = {10, 20, 30, 40, 50, 60};
arr gm : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
arr g2 : i32[3][2] = {{1, 2}, {3, 4}, {5, 6}};

fn suma(a : i32[6]) : i64 {
	return (a[0] + a[1] + a[2] + a[3] + a[4] + a[5]) as i64;
}

fn suma_slice(a : i32[]) : i64 {
	var t : i64 = 0;
	var i : i64 = 0;
	while (i < a.len) { t += a[i] as i64; i++; }
	return t;
}

fn primera(m : i32[2][]) : i64 { return m[0][0] as i64; }

pub fn main() : i64 {
	arr local : i32[6] = {1, 2, 3, 4, 5, 6};
	var a : i32[6] = local;
	var b : i32[6] = g;
	var c : i32[6];
	var v : i32[2][3] = gm;
	var x : i32[2][] = gm;
	var y : i32[3][] = g2;
	var z : i32[2][] = x;
	var la : i64 = a.len;
	var sa : i64 = sizeof(a);
	var sx : i64 = sizeof(x);
	var sy : i64 = sizeof(y);
	var ly : i64 = y[0].len;
	var lx : i64 = x.len;

	if (la != 6 || sa != 24 || a[5] != 6) { return 1; }
	if (suma(a) != 21 || suma(b) != 210) { return 2; }
	if (suma_slice(b) != 210 || suma_slice(a) != 21) { return 3; }
	a[3] = 40;
	if (local[3] != 40) { return 4; }
	b[0] = 11;
	if (g[0] != 11) { return 5; }
	c = a;
	if (c[3] != 40 || c[5] != 6) { return 6; }
	c = g;
	if (c[0] != 11 || c[5] != 60) { return 7; }
	c[0] = 12;
	if (g[0] != 12) { return 8; }
	if (v[1][2] != 6) { return 9; }
	v[1][2] = 60;
	if (gm[1][2] != 60) { return 10; }
	if (x[1][2] != 60 || lx != 2 || x[0].len != 3) { return 11; }
	if (sx != 16) { return 12; }
	if (y[2][1] != 6 || ly != 2 || sy != 16) { return 13; }
	x[1][2] = 66;
	if (gm[1][2] != 66) { return 14; }
	if (primera(x) != 1) { return 15; }
	x = gm;
	if (x[1][2] != 66) { return 16; }
	z = x;
	if (z[1][2] != 66) { return 17; }
	var p : ptr i32 = v;
	if (p[5] != 66) { return 18; }
	return 0;
}
