extern fn strlen (s : ptr char) : u64;
extern fn abs (x : i32) : i32;

fn measure (s : ptr char) : u64 {
	return strlen(s);
}

fn main () : i64 {
	var n : u64 = measure("hola mundo");
	var d : i32 = abs(-7);
	if (n != 10) { return 1; }
	if (d != 7) { return 2; }
	if (strlen("abc") != 3) { return 3; }
	return 0;
}