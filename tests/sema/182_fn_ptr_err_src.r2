fn suma (a : i32, b : i32) : i32 { return a + b; }
fn other (a : i64) : i64 { return a; }
fn dflt (a : i32, b : i32 = 2) : i32 { return a * b; }
fn tvar (a : i32, rest : ...i64) : i32 { return a; }

type Op = ptr fn(a : i32, b : i32) : i32;

fn ret_bad () : ptr fn(a : i32) : i32 {
	return suma;
}

fn takes (f : Op) : i32 {
	return f(1, 2);
}

var g_init : Op = tvar;
var g_var : Op;
var g_bad : Op = g_var;

pub fn main () : i64
{
	var p : Op = other;
	var q : ptr fn(a : i32) : i32 = NULL;
	var d : Op = dflt;
	p = suma;
	p = other;
	takes(other);
	p(1);
	p(1, 2, 3);
	p(a = 1, b = 2);
	var s : i32 = 1;
	var called : i32 = (s)(1);
	var sum1 : i64 = (p + 1) as i64;
	var sum2 : i64 = (p < q) as i64;
	var deref : i64 = @p as i64;
	var index : i64 = p[0] as i64;
	var addr : ptr ptr fn(a : i32, b : i32) : i32 = $suma;
	p += 1;
	suma = p;
	return 0;
}
