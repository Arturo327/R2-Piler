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

type Alias = Outer;

fn main () : i64 {
	var n : i64 = alignof(Outer);
	var m : i64 = alignof(n);
	var k : i64 = alignof("abc");
	var p : i64 = offsetof(Outer, c);
	var q : i64 = offsetof(Outer, inner.b);
	var r : i64 = offsetof(Alias, v);
	var s : i64 = alignof(u8) + offsetof(Inner, b) + sizeof(U);

	return n + m + k + p + q + r + s;
}
