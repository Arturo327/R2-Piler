struct Inner {
	a : u8;
	b : u32;
}

struct Outer {
	c : i8;
	inner : Inner;
	v : u16[3];
}

union U {
	x : i32;
	y : i64;
}

packed struct Pk {
	a : u8;
	b : u32;
}

struct Bits {
	lo : u8 : 3;
	hi : u8 : 5;
	k : i16;
}

type Alias = Outer;

var g_off : i64 = offsetof(Outer, inner.b);
var g_align : i64 = alignof(Alias);

fn main () : i64 {
	var o : Outer = {};
	var u : U = {};
	var bl : Bits = {};

	o.inner.b = 1;
	u.y = 2;
	bl.k = 3;

	return o.inner.b as i64 + u.y + bl.k as i64 + g_off + g_align
			+ alignof(o) + alignof(bl.hi) + offsetof(Pk, b) + offsetof(U, y)
			+ offsetof(Bits, k) + sizeof(Outer);
}
