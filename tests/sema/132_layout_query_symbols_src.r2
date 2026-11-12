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

type Alias = Outer;

struct Bits {
	lo : u8 : 3;
	hi : u8 : 5;
	k : i16;
}

var g_off : i64 = offsetof(Outer, inner.b);
var g_align : i64 = alignof(Alias);

fn main () : i64 {
	var o : Outer = {};
	var u : U = {};
	var pk : Pk = {};
	var bl : Bits = {};
	static var s : i64 = offsetof(U, y);

	o.inner.b = 5;
	u.y = 7;
	pk.b = 9;
	bl.k = 11;

	return g_off + g_align + s + alignof(o) + alignof(u) + alignof(pk) + alignof(bl.lo)
			+ alignof(bl.k) + offsetof(Pk, b) + offsetof(Inner, b) + offsetof(Outer, v)
			+ o.inner.b as i64 + u.y + pk.b as i64 + bl.k as i64;
}
