pub var g : i64 = 0;

pub struct Pair {
	a : i64;
	b : i32;
}

pub enum E {
	One = 1,
	Two = 2,
}

pub extern fn abs (x : i64) : i64;

pub fn multi (a : i64, b : u32) : i64, u32
{
	return a, b;
}

pub fn variadic (first : i64, _rest : ...i64) : i64
{
	return first;
}

pub fn refs (x : &i64, _items : i64[2], _sl : i64[]) : void
{
	x += 1;
}

pub fn defaults (_a : i64 = -1, _b : ptr i8 = NULL, _c : ptr i8 = "x",
		_d : ptr i64 = $g, _e : i64 = sizeof(i64),
		_pair : Pair = {.a = 1, .b = 2} as Pair, _en : E = E.Two) : i64
{
	return 0;
}

pub fn fnptr (cb : fn(x : i64) : i64 = abs) : i64
{
	return cb(1);
}

pub fn main () : i64
{
	return 0;
}