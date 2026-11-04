arr g : i32[3] = {1, 2, 3};
arr s : i8[6] = "hi";
arr z : i64[2];

fn sum(v : i32[]) : i64 {
	return (v[0] as i64) + v.len;
}

fn relay(v : i32[]) : i64 {
	return sum(v);
}

fn main() : i64 {
	arr a : i32[2] = {4, 5};
	var p : ptr i32 = a;
	var n : i64 = a.len;
	a[1] = 7;
	return relay(a) + sum(a) + (p[0] as i64) + n
		+ (g[2] as i64) + (s[1] as i64) + z[0];
}
