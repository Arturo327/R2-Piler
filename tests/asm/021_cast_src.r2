fn main() : i64 {
	var a : i64 = 300;
	var b : u8 = (a as u8) as u8;
	if ((b as i64) != 44) {
		return 1;
	}
	var c : i8 = -5;
	var d : i64 = c as i64;
	if (d != -5) {
		return 2;
	}
	var e : u8 = 255;
	var f : i64 = e as i64;
	if (f != 255) {
		return 3;
	}
	return 0;
}
