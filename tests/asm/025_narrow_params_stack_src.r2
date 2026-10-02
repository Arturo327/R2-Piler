fn f(a : i8, b : u8, c : i16, d : u16, e : i32, g : u32, h : i64) : i64 {
	return (a as i64) + (b as i64) + (c as i64) + (d as i64) + (e as i64) + (g as i64) + h;
}
fn main() : i64 {
	var r : i64 = f(1 as i8, 2 as u8, 3 as i16, 4 as u16, 5 as i32, 6 as u32, 7);
	if (r == 28) {
		return 0;
	}
	return 1;
}
