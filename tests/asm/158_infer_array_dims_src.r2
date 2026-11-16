arr g : i32[] = {3, 4, 5, 6};
arr gm : i32[][] = {{1, 2}, {2, 3}, {3, 4}};

pub fn main() : i64 {
	arr a : i32[] = {3, 4, 5, 6};
	arr m : i32[][] = {{1, 2}, {3, 4}, {5, 6}};
	arr n : i32[3][] = {{1, 2}, {4, 5}, {6, 7}};
	arr o : i32[][3] = {{1, 2, 3}, {4, 5, 6}};
	arr s : i8[] = "hola";
	static arr st : i64[] = {1, 2, 3, 4};
	var la : i64 = a.len;
	var lm : i64 = m.len;
	var lr : i64 = m[0].len;
	var ln : i64 = n.len;
	var lo : i64 = o.len;
	var ls : i64 = s.len;
	var lst : i64 = st.len;
	var lg : i64 = g.len;
	var lgm : i64 = gm[2].len;

	if (la != 4) { return 1; }
	if (lm != 3) { return 2; }
	if (lr != 2) { return 3; }
	if (m[2][1] != 6) { return 4; }
	if (ln != 3) { return 5; }
	if (n[1][1] != 5) { return 6; }
	if (lo != 2) { return 7; }
	if (o[1][2] != 6) { return 8; }
	if (ls != 5) { return 9; }
	if (s[4] != 0) { return 10; }
	if (lst != 4) { return 11; }
	if (st[3] != 4) { return 12; }
	if (lg != 4) { return 13; }
	if (lgm != 2) { return 14; }
	if (gm[2][1] != 4) { return 15; }
	if (a[0] + a[3] != 9) { return 16; }
	return 0;
}
