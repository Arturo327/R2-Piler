fn f(v : i32[]) : i64 {
	var a : u8 = v.len;
	var b : i32 = v.len;
	var c : i64 = v.len;

	return (a as i64) + (b as i64) + c;
}

fn main() : i64 {
	arr m : i32[7] = {1, 2, 3, 4, 5, 6, 7};
	return f(m);
}