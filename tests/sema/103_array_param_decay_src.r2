enum E : u8 { A, B }
newtype N = u16;

fn take27(x : i32[27]) : i64 {
	return (x[0] as i64) + x.len + sizeof(x[0]);
}

fn relay(x : i32[27]) : i64 {
	return take27(x) + ((x + 1)[0] as i64);
}

fn first(v : i32[]) : i64 {
	var p : ptr i32 = v + 1;
	var q : ptr i32 = 1 + v;
	var r : ptr i32 = v - 1;
	var d : i64 = (v + 2) - v;

	return (p[0] as i64) + (q[0] as i64) + (r[1] as i64) + d;
}

fn sized(e : E[2], n : N[2]) : i64 {
	return (e[1] as i64) + (n[1] as i64) + e.len;
}

fn main() : i64 {
	arr a : i32[27];
	arr b : i32[3] = {1, 2, 3};
	arr e : E[2] = {E.A, E.B};
	arr n : N[2] = {1, 2};
	var p : ptr i32 = b + 1;
	var q : ptr i32 = 1 + b;
	var r : ptr i32 = b - 1;
	var d : i64 = (b + 2) - b;
	var w : i64 = a.len + sizeof(a) + sizeof(a[0]);
	var t : i64 = take27(a) + relay(a) + first(b) + sized(e, n);

	return (p[0] as i64) + (q[0] as i64) + (r[1] as i64) + d + w + t;
}
