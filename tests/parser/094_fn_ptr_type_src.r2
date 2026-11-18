type Op = ptr fn(a : i32, b : u32) : i64;
type Bare = fn(x : i8);
type Multi = ptr fn(a : i32) : (i32, u32, ptr i8);
type Var = ptr fn(fmt : ptr i8, ...) : i32;
type Ref = ptr fn(p : &i64);

struct Handlers {
	op : ptr fn() : i32;
	many : (ptr fn(a : i64) : i64)[2];
}

fn takes (cb : ptr fn(a : i32) : i32) : ptr fn(a : i32) : i32 {
	return cb;
}

var g : ptr fn(a : i32, b : i32) : i32 = NULL;

pub fn main () : i64 {
	var local : fn(a : i32) : i32 = NULL;
	var nested : ptr ptr fn() = NULL;
	return 0;
}
