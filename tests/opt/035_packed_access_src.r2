packed struct Header {
	tag : u8;
	value : u32;
}

packed struct Bitmix {
	a : u8;
	lo : u32 : 3;
	hi : u32 : 29;
	c : u8;
	d : u16 : 12;
	e : u16 : 4;
}

struct Holder {
	h : Header;
	k : u8;
}

fn peek (p : ptr Header) : i64 {
	return p.tag as i64 + (p.value as i64);
}

fn make (t : u8, v : u32) : Header {
	var h : Header = {.tag = t, .value = v};

	return h;
}

fn main () : i64 {
	var h : Header = {.tag = 7, .value = 287454020};
	var c : Header = make(1, 2);
	var hp : ptr Header = $h;
	var m : Bitmix = {.a = 1, .lo = 5, .hi = 1023, .c = 2, .d = 4095, .e = 9};
	var ho : Holder = {.h = {.tag = 9, .value = 66051}, .k = 4};
	arr hs : Header[2];

	hs[0] = c;
	hs[1] = h;
	hp.value = 66051;
	m.lo = 3;
	m.d += 1;
	m.a++;

	if (hs[0].tag as i64 != 1) return 1;

	return peek(hp) + m.lo as i64 + m.hi as i64 + m.d as i64 + m.e as i64
			+ ho.h.tag as i64 + ho.k as i64;
}
