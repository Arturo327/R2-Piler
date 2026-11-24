extern fn my_alloc (n : u64) : ptr void;
extern fn my_free (p : ptr void) : void;

fn make (n : i64) : i32[][] {
	return new(my_alloc(16)) i32[n][2];
}

pub fn main() : i64 {
	var a : i32[] = new(my_alloc(8)) i32[2];
	var b : i32[][] = make(3);
	var p : ptr i32 = new(my_alloc(4)) i32;
	var q = new(my_alloc(4)) i32;
	delete(my_free) a;
	delete(my_free) b;
	delete(my_free) p;
	delete(my_free) q;
	return 0;
}
