extern fn snprintf (buf : ptr i8, n : i64, fmt : ptr i8, ...) : i32;

pub fn main () : i64
{
	arr buf : i8[32];
	var sp : ptr fn(buf : ptr i8, n : i64, fmt : ptr i8, ...) : i32 = snprintf;
	var n : i32 = sp(buf, 32, "%d-%d", 7, 5);

	return n as i64 + buf[0] as i64;
}
