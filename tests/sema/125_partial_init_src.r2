struct P {
	x : i32;
	y : i32;
	z : i8;
}

struct Q {
	p : P;
	n : i32;
}

struct WithArr {
	a : i32[3];
	k : i32;
}

struct Bits {
	lo : u3;
	hi : u5;
}

union V {
	struct { a : u8; b : u8; };
	c : u16;
}

struct Tagged {
	tag : u8;
	u : V;
}

packed struct PB {
	tag : u8;
	w : u32;
}

fn main () : i64 {
	var a : P = {};
	var b : P = {.x = 1};
	var c : Q = {.p = {.x = 2}};
	var d : WithArr = {.k = 3};
	var e : Bits = {.hi = 4};
	var f : V = {};
	var g : V = {.a = 5};
	var h : Tagged = {.u = {.b = 6}};
	var i : PB = {.w = 7};

	return (a.x + b.y + c.p.y + d.k + e.lo as i64 + f.c as i64
			+ g.c as i64 + h.u.b as i64 + i.tag as i64);
}
