import "stdlib";

pub fn main() : i64 {
	var m : i32[][] = new i32[2][3];
	var p : ptr i32 = new i32;
	@p = 1;
	delete m;
	delete p;
	return 0;
}
