arr g8 : i8[4] = {1, 2, 3, 4};
arr g16 : i16[4] = {10, 20, 30, 40};
arr g64 : i64[4] = {100, 200, 300, 400};

fn tail(v : i32[]) : i64 {
	var p : ptr i32 = v + 2;
	var q : ptr i32 = 2 + v;
	var r : ptr i32 = v - 1;
	var d : i64 = (v + 3) - v;

	if (p[0] != 3) { return 1; }
	if (q[0] != 3) { return 2; }
	if (r[1] != 1) { return 3; }
	if (d != 3) { return 4; }

	return (p[0] as i64) + (q[0] as i64) + (r[1] as i64) + d;
}

fn main() : i64 {
	arr a : i32[4] = {1, 2, 3, 4};
	var i : i64 = 0;
	var sum : i64 = 0;
	var p : ptr i32 = a + 2;
	var q : ptr i32 = 2 + a;
	var r : ptr i32 = a - 1;
	var d : i64 = (a + 3) - a;
	var b8 : ptr i8 = g8 + 1;
	var b16 : ptr i16 = 1 + g16;
	var b64 : ptr i64 = g64 + 2;
	var e64 : i64 = (g64 + 3) - g64;

	if (p[0] != 3) { return 5; }
	if (q[0] != 3) { return 6; }
	if (r[1] != 1) { return 7; }
	if (d != 3) { return 8; }
	if (b8[0] != 2) { return 9; }
	if (b16[0] != 20) { return 10; }
	if (b64[0] != 300) { return 11; }
	if (e64 != 3) { return 12; }
	if (tail(a) != 10) { return 13; }

	while (i < 1000) {
		sum += ((a + (i % 4))[0] as i64);
		i++;
	}
	if (sum != 2500) { return 14; }
	return 0;
}
