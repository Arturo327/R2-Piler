struct Bits {
	a : u3;
	b : i5;
	c : u4;
	d : u8 : 8;
}

struct Wide {
	lo : u16 : 16;
	hi : u32 : 32;
	big : u64 : 64;
}

struct Wide32 {
	lo : u32 : 5;
	hi : u32 : 12;
	neg : i32 : 5;
}

union Pack {
	whole : u32;
	part : u12;
}

fn mix (x : u8, y : i8) : i64 {
	var f : Bits = {.a = x, .b = y, .c = 0, .d = 0};

	f.a += 6;
	f.b = f.b - 1;
	return f.a as i64 + f.b as i64;
}

fn main () : i64 {
	var v : Bits = {.a = 7, .b = -16, .c = 15, .d = 200};
	var w : Wide = {.lo = 65535, .hi = 4294967295, .big = 1};
	var u : Pack = {.part = 4095};
	var wrap : i8 = 16;
	var neg : i8 = -20;
	var over : u8 = 20;
	var wrap32 : u32 = 33;
	var over32 : i32 = 16;

	if (v.a as i64 != 7) return 1;
	if (v.b as i64 != -16) return 2;
	if (v.c as i64 != 15) return 3;
	if (v.d as i64 != 200) return 4;

	v.b = wrap;
	if (v.b as i64 != -16) return 5;
	v.b = neg;
	if (v.b as i64 != 12) return 6;
	v.c = over;
	if (v.c as i64 != 4) return 7;
	v.a += 7;
	if (v.a as i64 != 6) return 8;
	v.a++;
	if (v.a as i64 != 7) return 9;
	v.a--;
	v.a--;
	if (v.a as i64 != 5) return 10;
	if (v.d as i64 != 200) return 11;
	if (v.c as i64 != 4) return 12;

	w.lo = 1000;
	w.hi = 3000000000;
	w.big = 123456789012345;
	if (w.lo as i64 != 1000) return 13;
	if (w.hi as i64 != 3000000000) return 14;
	if (w.big as i64 != 123456789012345) return 15;

	u.part = 4095;
	if (u.whole as i64 != 4095) return 16;
	u.whole = 4294967295;
	if (u.part as i64 != 4095) return 17;

	var copy : Bits = v;
	copy.a = 0;
	if (v.a as i64 != 5) return 18;
	if (copy.b as i64 != 12) return 19;

	if (mix(5, 3) != 5) return 20;
	if (mix(7, 15) != 19) return 21;

	var wd : Wide32 = {.lo = 31, .hi = 4095, .neg = -3};
	var sz32 : i64 = sizeof(Wide32) as i64;

	if (wd.lo as i64 != 31) return 22;
	if (wd.hi as i64 != 4095) return 23;
	if (wd.neg as i64 != -3) return 24;
	if (sz32 != 4) return 25;

	wd.lo = wrap32;
	if (wd.lo as i64 != 1) return 26;
	if (wd.hi as i64 != 4095) return 27;
	if (wd.neg as i64 != -3) return 28;

	wd.neg = over32;
	if (wd.neg as i64 != -16) return 29;
	if (wd.lo as i64 != 1) return 30;
	if (wd.hi as i64 != 4095) return 31;

	wd.hi = 0;
	if (wd.hi as i64 != 0) return 32;
	if (wd.lo as i64 != 1) return 33;
	if (wd.neg as i64 != -16) return 34;

	return 0;
}
