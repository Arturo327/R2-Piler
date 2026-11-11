struct Flags {
	a : u3;
	b : i5;
	c : u8;
	d : u16 : 16;
}

struct Closed {
	a : u3;
	b : u8;
	c : u5;
}

struct Nested {
	head : u8;
	struct { lo : u4; hi : u4; };
	tail : u2;
}

struct Wide {
	lo : u32 : 5;
	hi : u32 : 12;
	sign : i32 : 5;
	tail : u8;
}

struct Narrow {
	a : u8 : 1;
	b : u8 : 7;
	c : u16 : 3;
	d : u32 : 17;
}

union Over {
	whole : u32;
	lo : u8;
	hi : u16 : 16;
}

struct HasUnion {
	tag : u8;
	union { small : u8; pair : u16; };
	last : u2;
}

fn main () : i64 {
	var f : Flags = {.a = 1, .b = 2, .c = 3, .d = 4};
	var c : Closed = {.a = 1, .b = 2, .c = 3};
	var n : Nested = {.head = 1, .lo = 2, .hi = 3, .tail = 1};
	var wd : Wide = {.lo = 1, .hi = 2, .sign = 3, .tail = 4};
	var nr : Narrow = {.a = 1, .b = 2, .c = 3, .d = 4};
	var o : Over = {.whole = 1};
	var h : HasUnion = {.tag = 1, .pair = 2, .last = 1};

	return f.a as i64 + c.c as i64 + n.head as i64 + o.lo as i64 + h.tag as i64
			+ wd.lo as i64 + nr.d as i64;
}
