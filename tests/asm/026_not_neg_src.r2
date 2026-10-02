fn main() : i64 {
	var a : i8 = 5;
	var b : i8 = -a;
	if (b != -5) {
		return 1;
	}
	var c : u8 = 240;
	var d : u8 = (~c) as u8;
	if ((d as i64) != 15) {
		return 2;
	}
	var e : i64 = 0;
	if ((!e) != 1) {
		return 3;
	}
	return 0;
}
