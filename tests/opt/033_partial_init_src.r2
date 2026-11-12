struct P {
	x : i32;
	y : i32;
	z : i8;
}

struct Q {
	p : P;
	n : i32;
}

union V {
	struct { a : u8; b : u8; };
	c : u16;
}

struct Bits {
	lo : u3;
	hi : u5;
}

struct WithArr {
	a : i32[3];
	k : i32;
}

packed struct Big {
	head : i64;
	body : u8[61];
	tail : i32;
}

fn main () : i64 {
	var a : P = {};
	var b : P = {.x = 1};
	var c : Q = {.p = {.x = 2}};
	var d : V = {};
	var e : V = {.a = 3};
	var f : Bits = {.lo = 4};
	var g : WithArr = {.k = 5};
	var h : Big = {.tail = 6};
	var i : P = {.x = 7, .y = 8, .z = 9};

	return a.x as i64 + b.x as i64 + c.p.x as i64 + d.c as i64 + e.c as i64
			+ f.lo as i64 + g.k as i64 + (h.tail as i64) + i.x as i64;
}