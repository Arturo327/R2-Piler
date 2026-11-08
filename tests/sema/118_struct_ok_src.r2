struct Inner {
	a: u8;
	b: u32;
}

struct Outer {
	x: i8;
	inner: Inner;
	tags: u16[3];
}

struct Cpu {
	tag: i8;
	union {
		whole: u32;
		struct {
			lo: u16;
			hi: u16;
		}
	}
}

union Value {
	i: i64;
	b: u8[8];
}

var g : Outer = {
	.x = 1,
	.inner = {.a = 2, .b = 3},
	.tags = {4, 5, 6}
};

fn take(v : Outer) : Outer {
	return v;
}

fn main() : i64 {
	var o : Outer = g;
	var p : ptr Outer = $o;
	arr ps : Outer[2] = {g, g};

	o = take(o);
	p.inner.b = 9;
	ps[0].inner.a = ps[1].inner.a + 1;
	if (o.inner.b != 9) return 1;
	return ps[1].x as i64;
}
