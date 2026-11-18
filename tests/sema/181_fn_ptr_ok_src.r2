type Op = ptr fn(a : i32, b : i32) : i32;

struct Table {
	op : Op;
	name : ptr i8;
}

fn suma (a : i32, b : i32) : i32 { return a + b; }
extern fn printf (fmt : ptr i8, ...) : i32;
fn pair (a : i32) : i32, u32 { return a, a as u32; }
fn mk () : Table { return {.op = suma, .name = "s"} as Table; }
fn apply (f : Op, x : i32, y : i32) : i32 { return f(x, y); }
fn pick (flag : i32) : Op { return flag ? suma : NULL; }
fn bump (v : &i64) { v = v + 1; }

var gsum : Op = suma;
var gvar : ptr fn(fmt : ptr i8, ...) : i32 = printf;

pub fn main () : i64
{
	var local : Op = suma;
	var impl : fn(a : i32, b : i32) : i32 = local;
	var as_void : ptr void = local;
	var back : Op = as_void;
	var tbl : Table = mk();
	var a : i32 = apply(local, 1, 2);
	var b : i32 = local(3, 4);
	var c : i32 = tbl.op(5, 6);
	var d : i32 = pick(1)(7, 8);
	var m1, m2 = pair(9);
	var v : i64 = 1;
	var rp : ptr fn(x : ptr i64) = bump;
	rp($v);
	as_void = gvar;
	local = gsum;
	if (local == gsum) a += 1;
	if (local != NULL) a += 1;
	var sz : i64 = sizeof(Op) as i64 + alignof(Op) as i64;
	return a as i64 + b as i64 + c as i64 + d as i64 + m1 as i64
			+ m2 as i64 + v + sz + impl(1, 1) as i64 + back(10, 10) as i64;
}
