fn main() : i64 {
	var a : u8 = 200;
	var b : u8 = 100;
	var c : u8 = (a + b) as u8;
	if ((c as i64) == 44) {
		return 0;
	}
	return 1;
}
