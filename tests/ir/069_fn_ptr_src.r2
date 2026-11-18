type Op = ptr fn(a : i32, b : i32) : i32;

fn suma (a : i32, b : i32) : i32 { return a + b; }
fn resta (a : i32, b : i32) : i32 { return a - b; }

struct Holder {
	op : Op;
	n : i32;
}

var g : Op = suma;

pub fn main () : i64
{
	var local : Op = resta;
	var h : Holder = {.op = suma, .n = 3};
	var a : i32 = local(1, 2);
	var b : i32 = g(3, 4);
	var c : i32 = h.op(5, 6);
	return a as i64 + b as i64 + c as i64;
}
