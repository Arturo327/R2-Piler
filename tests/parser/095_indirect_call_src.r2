struct S {
	cb : ptr fn(a : i32) : i32;
}

fn plain (a : i32) : i32 { return a; }

pub fn main () : i64 {
	arr table : (ptr fn(a : i32) : i32)[2] = {plain, plain};
	var s : S = {.cb = plain};
	var cond : i32 = 1;
	var a : i32 = table[0](1);
	var b : i32 = s.cb(2);
	var c : i32 = (cond ? plain : table[1])(3);
	var d : i32 = s.cb(4);
	return a as i64 + b as i64 + c as i64 + d as i64;
}
