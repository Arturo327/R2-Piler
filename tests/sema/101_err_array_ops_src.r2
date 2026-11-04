fn set_len(v : i32[]) : void {
	v.len = 3;
}

fn main() : i64 {
	arr a : i32[3] = {1, 2, 3};
	arr b : i32[3] = {4, 5, 6};
	var x : i64 = 0;

	a = b;
	a += b;
	a++;
	x = -a;
	x = a + b;
	if (a) { return 1; }
	if (!a) { return 2; }
	a.len = 3;
	var y : i64 = a.len;
	var z : i64 = b.foo;
	var w : ptr i32 = a as ptr i32;
	var q : ptr i32 = $a;
	return a[3] + a[-1] + b[a] + y;
}
