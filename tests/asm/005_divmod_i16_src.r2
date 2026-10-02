fn main() : i64 {
	var a : i16 = -1000;
	var b : i16 = 7;
	var c : i16 = (a / b) as i16;
	var d : i16 = (a % b) as i16;
	if (c == -142) {
		if (d == -6) {
			return 0;
		}
		return 2;
	}
	return 1;
}
