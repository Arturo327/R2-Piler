enum E : u8 { A, B }
newtype N = u16;

arr gm : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

fn total(v : i32[]) : i64 {
	return (v[0] as i64) + v.len + sizeof(v);
}

fn sum(m : i32[][]) : i64 {
	return (m[1][2] as i64) + m.len + m[0].len + total(m[0])
			+ sizeof(m) + sizeof(m[0]);
}

fn fixed(m : i32[2][3]) : i64 {
	var p : ptr i32 = m + 1;

	return (p[0] as i64) + m.len + m[0].len;
}

fn main() : i64 {
	arr a : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
	arr s : i8[2][4] = {"ab", "cd"};
	arr e : E[2][2] = {{E.A, E.B}, {E.B, E.A}};
	arr n : N[3][2] = {{1, 2}, {3, 4}, {5, 6}};
	var p : ptr i32 = a;
	var q : ptr i32 = a[1];
	var w : i64 = a.len + a[1].len;
	var z : i64 = sizeof(a) + sizeof(a[0]) + sizeof(a[0][0])
			+ sizeof(s) + sizeof(e) + sizeof(n) + sizeof(i32[2][3]);
	var t : i64 = sum(a) + fixed(a) + total(a[0])
			+ (p[1] as i64) + (q[2] as i64);
	return t + w + z + (s[0][1] as i64) + (e[1][0] as i64)
			+ (n[2][1] as i64) + (gm[1][1] as i64);
}
