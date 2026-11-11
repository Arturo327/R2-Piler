struct Bits {
	a : u3;
	b : i5;
	c : u4;
	d : u8 : 8;
}

struct Packed32 {
	lo : u32 : 5;
	hi : u32 : 12;
	sign : i32 : 5;
}

fn main () : i64 {
	var v : Bits = {.a = 5, .b = -3, .c = 9, .d = 7};
	var w : Packed32 = {.lo = 31, .hi = 4095, .sign = -3};

	v.b = 7;
	v.c += 2;
	v.a++;
	v.b--;
	v.d = 3;
	w.lo = 3;
	w.sign = 7;

	return v.a as i64 + v.b as i64 + v.d as i64 + w.hi as i64 + w.sign as i64;
}
