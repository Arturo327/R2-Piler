enum E : u8 { A, B }
newtype N = u16;

arr g : i32[3] = {1, 2, 3};
arr s : i8[8] = "hi";
arr u : u8[4] = "ok";
arr z : i64[2];
arr e : E[2] = {E.A, E.B};
arr nn : N[2] = {1, 2};

fn sum(v : i32[]) : i64 {
	return (v[0] as i64) + v.len;
}

fn main() : i64 {
	arr a : i32[4] = {1, 2, 3, 4};
	var p : ptr i32 = a;
	var q : ptr i32 = a;
	var n : i64 = a.len;
	var zs : i64 = sizeof(a) + sizeof(a[0]) + sizeof(i32[3]);
	var w : u64 = a.len;
	var t : i64 = sum(a) + (p[1] as i64) + (q[2] as i64);
	return t + n + zs + (w as i64) + (s[2] as i64) + (u[2] as i64)
		+ (g[2] as i64) + z[0] + (e[1] as i64) + (nn[1] as i64);
}
