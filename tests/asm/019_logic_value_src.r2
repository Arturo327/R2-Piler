fn main() : i64 {
	var a : i64 = 1;
	var b : i64 = 0;
	var c : i64 = (a && b) as i64;
	if (c != 0) {
		return 1;
	}
	var d : i64 = (a || b) as i64;
	if (d != 1) {
		return 2;
	}
	if ((!b) != 1) {
		return 3;
	}
	return 0;
}
