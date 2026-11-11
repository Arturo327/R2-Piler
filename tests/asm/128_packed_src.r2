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

packed union Bits {
	whole : u32;
	parts : u8[4];
}

struct Outer {
	h : Header;
	o : u8;
}

packed struct Holder {
	n : Outer;
	k : u8;
}

fn hsum (h : Header) : i64 {
	return h.tag as i64 + h.value as i64;
}

fn hmake (t : u8, v : u32) : Header {
	var h : Header = {.tag = t, .value = v};

	return h;
}

fn main () : i64 {
	var h : Header = {.tag = 7, .value = 287454020};
	var p : ptr u8 = $h as ptr u8;
	var m : Bitmix = {.a = 1, .lo = 5, .hi = 1023, .c = 2, .d = 1000, .e = 9};
	var mp : ptr u8 = $m as ptr u8;
	var bi : Bits = {.whole = 287454020};
	var o : Outer = {.h = {.tag = 9, .value = 66051}, .o = 4};
	var ho : Holder = {.n = {.h = {.tag = 3, .value = 131072}, .o = 8}, .k = 6};
	var sz : i64 = sizeof(Header) as i64;

	if (sz != 5) return 1;
	sz = sizeof(Bitmix) as i64;
	if (sz != 8) return 2;
	sz = sizeof(Bits) as i64;
	if (sz != 4) return 3;
	sz = sizeof(Outer) as i64;
	if (sz != 6) return 4;
	sz = sizeof(Holder) as i64;
	if (sz != 7) return 5;

	if (@p as i64 != 7) return 6;
	if (@(p + 1) as i64 != 68) return 7;
	if (@(p + 2) as i64 != 51) return 8;
	if (@(p + 3) as i64 != 34) return 9;
	if (@(p + 4) as i64 != 17) return 10;

	if (@mp as i64 != 1) return 11;
	if (@(mp + 1) as i64 != 253) return 12;
	if (@(mp + 2) as i64 != 31) return 13;
	if (@(mp + 3) as i64 != 0) return 14;
	if (@(mp + 4) as i64 != 0) return 15;
	if (@(mp + 5) as i64 != 2) return 16;
	if (@(mp + 6) as i64 != 232) return 17;
	if (@(mp + 7) as i64 != 147) return 18;

	var hp : ptr Header = $h;
	hp.value = 66051;
	if (h.value != 66051) return 19;
	if (@(p + 1) as i64 != 3) return 20;
	if (@(p + 2) as i64 != 2) return 21;
	hp.tag = 8;
	if (@p as i64 != 8) return 22;

	if (hsum(h) != 66059) return 23;
	var c : Header = hmake(1, 287454020);
	if (c.tag as i64 != 1) return 24;
	if (c.value != 287454020) return 25;

	arr hs : Header[2];
	hs[0] = c;
	hs[1] = h;
	if (hs[0].tag as i64 != 1) return 26;
	if (hs[1].tag as i64 != 8) return 27;
	if (hs[1].value != 66051) return 28;
	var hb : ptr Header = hs;
	var q : ptr u8 = hb as ptr u8;
	if (($hs[1] as ptr u8) - q != 5) return 29;
	if (@(q + 5) as i64 != 8) return 30;
	if (@(q + 9) as i64 != 0) return 31;

	m.lo = 3;
	if (m.lo as i64 != 3) return 32;
	if (m.hi as i64 != 1023) return 33;
	if (m.a as i64 != 1) return 34;
	if (m.c as i64 != 2) return 35;
	if (@(mp + 1) as i64 != 251) return 36;
	m.d += 1;
	if (m.d as i64 != 1001) return 37;
	if (m.e as i64 != 9) return 38;
	if (@(mp + 6) as i64 != 233) return 39;
	if (@(mp + 7) as i64 != 147) return 40;
	m.e = 15;
	if (@(mp + 7) as i64 != 243) return 41;
	m.e++;
	if (m.e as i64 != 0) return 42;
	if (@(mp + 7) as i64 != 3) return 43;
	m.a++;
	if (@mp as i64 != 2) return 44;

	if (bi.parts[0] as i64 != 68) return 45;
	if (bi.parts[3] as i64 != 17) return 46;
	bi.parts[1] = 0;
	if (bi.whole != 287440964) return 47;

	if (o.h.tag as i64 != 9) return 48;
	if (o.h.value != 66051) return 49;
	if (o.o as i64 != 4) return 50;
	var op : ptr u8 = $o as ptr u8;
	if (@(op + 5) as i64 != 4) return 51;
	if (ho.n.h.value != 131072) return 52;
	if (ho.n.o as i64 != 8) return 53;
	if (ho.k as i64 != 6) return 54;
	var hop : ptr u8 = $ho as ptr u8;
	if (@(hop + 3) as i64 != 2) return 55;
	if (@(hop + 6) as i64 != 6) return 56;

	return 0;
}
