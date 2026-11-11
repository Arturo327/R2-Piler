packed struct Header {
	tag : u8;
	value : u32;
}

packed union Bits {
	whole : u32;
	parts : u8[4];
	half : u16;
}

packed struct Bitmix {
	a : u8;
	b : u32 : 3;
	c : u8;
	d : u16 : 12;
}

packed struct Nested {
	head : u8;
	struct { lo : u4; hi : u4; };
	tail : u8 : 3;
}

struct Holder {
	h : Header;
	k : u8;
}

packed struct Wrapper {
	p : Held;
	q : u8;
}

struct Held {
	a : u8;
	b : u32;
}

type Alias = Header;

struct Uses {
	h : Header;
	p : Held;
	t : Alias;
}

fn main () : i64 {
	var h : Header = {.tag = 1, .value = 2};
	var b : Bits = {.whole = 3};
	var m : Bitmix = {.a = 1, .b = 2, .c = 3, .d = 4};
	var n : Nested = {.head = 1, .lo = 2, .hi = 3, .tail = 4};
	var ho : Holder = {.h = {.tag = 1, .value = 2}, .k = 3};
	var w : Wrapper = {.p = {.a = 1, .b = 2}, .q = 3};
	var u : Uses = {.h = {.tag = 1, .value = 2}, .p = {.a = 3, .b = 4}, .t = {.tag = 5, .value = 6}};

	return h.tag as i64 + b.whole as i64 + m.b as i64 + n.tail as i64 + ho.k as i64
			+ w.q as i64 + u.t.value as i64;
}
