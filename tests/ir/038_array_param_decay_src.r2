fn take(x : i32[3]) : i64 {
	var p : ptr i32 = x + 1;

	return p[0] as i64;
}

fn relay(v : i32[]) : i64 {
	var q : ptr i32 = v + 1;

	return (q - v) + (q[0] as i64);
}

fn main() : i64 {
	arr a : i32[3] = {1, 2, 3};

	return take(a) + relay(a);
}
