import "stdlib";

fn total (v : i32[]) : i64 {
	var t : i64 = 0;
	var i : i64 = 0;

	while (i < v.len) { t += v[i] as i64; i++; }
	return t;
}

pub fn main () : i64 {
	var buf : i32[] = new i32[1000];
	var i : i64 = 0;

	while (i < 1000) { buf[i] = (i % 7) as i32; i++; }
	var expect : i64 = 0;

	i = 0;
	while (i < 1000) { expect += i % 7; i++; }
	if (buf.len != 1000) return 1;
	if (total(buf) != expect) return 2;
	buf[0] = 100;
	if (buf[0] != 100) return 3;

	var p : ptr i32 = new i32;

	@p = 42;
	if (@p != 42) return 4;
	delete buf;
	delete p;
	return 0;
}
