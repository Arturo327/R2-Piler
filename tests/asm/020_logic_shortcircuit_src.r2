var cnt : i64 = 0;
fn inc(v : i64) : i64 {
	cnt = cnt + 1;
	return v;
}
fn main() : i64 {
	var a : i64 = (inc(0) && inc(1)) as i64;
	if (cnt != 1) {
		return 1;
	}
	if (a != 0) {
		return 2;
	}
	var b : i64 = (inc(1) || inc(0)) as i64;
	if (cnt != 2) {
		return 3;
	}
	if (b != 1) {
		return 4;
	}
	return 0;
}
