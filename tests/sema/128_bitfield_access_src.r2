struct Bits {
	a : u3;
	b : i5;
	c : u8;
}

fn read (x : ptr Bits) : i64 {
	return (@x).a as i64 + (@x).b as i64 + (@x).c as i64;
}

fn main () : i64 {
	var v : Bits = {.a = 1, .b = 2, .c = 3};
	var p : ptr Bits = $v;
	var s : u8;
	var t : i8;

	p.a = 5;
	v.b = v.b + 1;
	p.c += 2;
	v.a++;
	v.b--;
	s = v.a;
	t = v.b;
	if (v.a > s) { t = 1; }
	if (v.a == 0) { v.a = 1; }
	var n : i64 = sizeof(v.a) as i64 + sizeof(v.b) as i64 + sizeof(v.c) as i64;
	var bad : ptr u8 = $v.a;

	if (bad == NULL) t = 0;

	return read(p) + t as i64 + n;
}
