export fn c_entry (x : i32) : i32
{
	return x;
}

export var c_count : i64 = 1;
export arr c_table : i64[2] = {1, 2};

pub extern fn puts (s : ptr i8) : i32;
pub extern var errno : i32;

extern fn private_c (x : i32) : i32;

pub fn main () : i64
{
	return 0;
}