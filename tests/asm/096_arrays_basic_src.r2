extern fn strlen(s : ptr i8) : u64;

arr g : i32[4] = {7, 8, 9, 10};
arr gs : i8[8] = "hi";
arr gz : i64[3];

fn main() : i64 {
	arr a : i32[4] = {1, 2, 3, 4};
	arr s : i8[8] = "ok";
	var i : i64 = 0;
	var sum : i64 = 0;

	while (i < 1000) {
		sum += (a[i % 4] as i64);
		i++;
	}
	if (sum != 2500) { return 1; }
	a[0] += 10;
	a[1]++;
	if (a[0] != 11) { return 2; }
	if (a[1] != 3) { return 3; }
	var n : i64 = a.len;
	var sz : i64 = sizeof(a);
	if (n != 4) { return 4; }
	if (sz != 16) { return 5; }
	if (s[2] != 0 || s[1] != 107) { return 6; }
	if (gs[2] != 0 || gs[1] != 105) { return 7; }
	gz[2] = 30;
	if (gz[2] != 30 || gz[0] != 0) { return 8; }
	if (g[0] != 7 || g[3] != 10) { return 9; }
	var gl : i64 = g.len + gs.len + gz.len;
	if (gl != 15) { return 10; }
	if (strlen(s) != 2 || strlen(gs) != 2) { return 11; }
	return 0;
}
