extern fn my_alloc (n : u64) : ptr void;

type Slice = i32[];

fn no_args () : void {}
fn takes_i64 (p : ptr i64) : void {}
fn ret_slice (p : ptr void) : i32[] { var s : i32[] = new(p) i32[1]; return s; }

pub fn main() : i64 {
	var a : i32[] = new i32[4];
	var b : i32[] = new(4) i32[4];
	var c : Slice[] = new Slice[3];
	var d : i32 = 3;
	delete a;
	delete d;
	delete(no_args) a;
	delete(takes_i64) a;
	delete(ret_slice) a;
	return 0;
}
