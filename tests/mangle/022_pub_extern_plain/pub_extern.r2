pub extern fn puts (s : ptr i8) : i32;

pub fn main () : i64
{
	return puts(NULL) as i64;
}