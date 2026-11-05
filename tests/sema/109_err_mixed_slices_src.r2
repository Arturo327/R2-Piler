fn dos (m : i32[][3]) : i64 { return m[0][0] as i64; }

fn dos_dyn (m : i32[2][]) : i64 { return m[0][0] as i64; }

fn main () : i64
{
	arr a : i32[2][4] = { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } };
	arr b : i32[2][3][2];
	arr c : i32[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
	var t : i64 = 0;

	t += dos(a);
	t += dos(b);
	t += dos_dyn(c);
	t += dos_dyn(b);
	return t;
}
