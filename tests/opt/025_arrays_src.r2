arr g : i32[2] = {3, 4};

fn main() : i64 {
	arr a : i32[3] = {1, 2, 3};
	var p : ptr i32 = a;
	var n : i64 = a.len;

	a[1] = 5;
	return (a[0] as i64) + (p[1] as i64) + n + (g[1] as i64);
}
