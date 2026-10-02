fn main() : i64 {
	var a : u8 = 1;
	var n : i64 = 9;
	var b : u8 = (a << n) as u8;
	if ((b as i64) != 0) {
		return 1;
	}
	var c : i8 = -128;
	var d : i8 = (c >> n) as i8;
	if (d != -1) {
		return 2;
	}
	var e : u8 = 255;
	var f : u8 = (e >> n) as u8;
	if ((f as i64) != 0) {
		return 3;
	}
	return 0;
}
