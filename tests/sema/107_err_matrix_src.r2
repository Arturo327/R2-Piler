fn total(v : i32[]) : i64 {
	return v.len;
}

fn sum(m : i32[][]) : i64 {
	return m.len;
}

fn bad(p : ptr i32, a : i32[2], m : i32[2][3], s : i32[]) : i64 {
	var t : i64 = sum(a);
	var u : i64 = sum(p);
	var w : i64 = total(m);
	var x : i64 = total(s);
	var y : i64 = sum(m);
	var z : i64 = m[0];
	var o : i64 = m[2][0] as i64;
	var q : i64 = m[0][3] as i64;

	return t + u + w + x + y + z + o + q;
}

fn main() : i64 {
	arr a : i32[2][];
	arr b : i32[][3];
	arr c : i32[][][];

	return sizeof(i32) + sizeof(i32[2][3]);
}
