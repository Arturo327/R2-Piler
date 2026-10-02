fn main() : i64 {
	var a : u8 = 1;
	var b : i64 = 100;
	var c : u16 = 2;
	var d : i32 = 3;
	var e : i8 = -4;
	var s : i64 = (a as i64) + b + (c as i64) + (d as i64) + (e as i64);
	if (s == 102) {
		return 0;
	}
	return 1;
}
