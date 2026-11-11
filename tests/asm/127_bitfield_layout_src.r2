struct Flags {
	a : u3;
	b : i5;
	c : u8;
	d : u16 : 16;
}

struct Nested {
	hdr : u8;
	struct { lo : u4; hi : u4; };
	tail : u2;
}

struct Wide32 {
	lo : u32 : 5;
	hi : u32 : 12;
	neg : i32 : 5;
}

union Over {
	whole : u32;
	lo : u8;
	half : u16 : 16;
}

fn main () : i64 {
	var f : Flags = {.a = 5, .b = -3, .c = 171, .d = 4660};
	var p : ptr u8 = $f as ptr u8;
	var n : Nested = {.hdr = 17, .lo = 10, .hi = 5, .tail = 2};
	var q : ptr u8 = $n as ptr u8;
	var o : Over = {.whole = 287454020};
	var o2 : Over = {.lo = 255};
	var wd : Wide32 = {.lo = 31, .hi = 4095, .neg = -3};
	var r : ptr u8 = $wd as ptr u8;
	var sz : i64 = sizeof(Flags) as i64;

	if (sz != 4) return 1;
	sz = sizeof(Nested) as i64;
	if (sz != 3) return 2;
	sz = sizeof(Over) as i64;
	if (sz != 4) return 3;

	if (@p as i64 != 237) return 4;
	if (@(p + 1) as i64 != 171) return 5;
	if (@(p + 2) as i64 != 52) return 6;
	if (@(p + 3) as i64 != 18) return 7;

	if (@q as i64 != 17) return 8;
	if (@(q + 1) as i64 != 90) return 9;
	if ((@(q + 2) as i64 & 3) != 2) return 10;

	if (o.lo as i64 != 68) return 11;
	if (o.half as i64 != 13124) return 12;
	if (o2.whole as i64 != 255) return 13;

	if (@r as i64 != 255) return 14;
	if (@(r + 1) as i64 != 255) return 15;
	if (@(r + 2) as i64 != 59) return 16;
	if (@(r + 3) as i64 != 0) return 17;
	sz = sizeof(Wide32) as i64;
	if (sz != 4) return 18;

	return 0;
}
