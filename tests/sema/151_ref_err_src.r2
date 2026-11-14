struct B {
	f : u8 : 3;
}

fn inc(x : &i64) {
	x += 1;
}

fn afn(x : &i32[3]) {
	x[0] = 1;
}

fn sfn(x : &i32[]) {
	x[0] = 1;
}

fn dflt(x : &i64 = 5) {
	x += 1;
}

fn deref(x : &i64) : i64 {
	return @x;
}

fn bf(x : &u8) {
	x += 1;
}

fn main() : i64 {
	var n : i64 = 0;
	inc(5);
	var m : i32 = 0;
	inc(m);
	var b : B = {.f = 1};
	bf(b.f);
	var v : i32[3] = {1, 2, 3};
	afn(v);
	sfn(v);
	dflt(n);
	if (v[0] == 0) return 1;
	return deref(n);
}
