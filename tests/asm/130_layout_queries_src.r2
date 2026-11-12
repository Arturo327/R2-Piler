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

packed struct Pk {
	a : u8;
	b : u32;
}

type Alias = Outer;

struct Cpu {
	tag : i8;
	union {
		whole : u32;
		struct { lo : u16; hi : u16; }
	}
}

struct Bits {
	lo : u8 : 3;
	hi : u8 : 5;
	k : i16;
}

fn main () : i64 {
	var o : Outer = {};
	var p : ptr Outer = $o;
	var st : ptr i8 = "abc";
	var bl : Bits = {};
	var c : Cpu = {};

	var t1 : i64 = alignof(i32);
	var t2 : i64 = alignof(u8);
	var t3 : i64 = alignof(Outer);
	var t4 : i64 = alignof(Alias);
	var t5 : i64 = alignof(o);
	var t6 : i64 = alignof(o.inner);
	var t7 : i64 = alignof(o.inner.a);
	var t8 : i64 = alignof(o.v);
	var t9 : i64 = alignof(p);
	var t10 : i64 = alignof("abc");
	var t11 : i64 = alignof(st);
	var t12 : i64 = alignof(ptr i64);
	var t13 : i64 = alignof(i32[]);
	var t14 : i64 = alignof(i32[4]);
	var t15 : i64 = alignof(NULL);
	var t16 : i64 = alignof(1);
	var t17 : i64 = alignof('a');
	var t18 : i64 = alignof(U);
	var t19 : i64 = alignof(Pk);
	var t20 : i64 = alignof(bl.lo);
	var t21 : i64 = alignof(bl.k);
	var t22 : i64 = alignof(c);
	var t23 : i64 = sizeof(Pk);
	var t24 : i64 = sizeof(Outer);
	var t25 : i64 = alignof(u8[]);
	var t26 : i64 = alignof(i32[][]);
	var t27 : i64 = sizeof(i32[]);

	var e1 : i64 = offsetof(Outer, c);
	var e2 : i64 = offsetof(Outer, inner);
	var e3 : i64 = offsetof(Outer, inner.b);
	var e4 : i64 = offsetof(Outer, v);
	var e5 : i64 = offsetof(Alias, inner.b);
	var e6 : i64 = offsetof(U, x);
	var e7 : i64 = offsetof(U, y);
	var e8 : i64 = offsetof(Pk, b);
	var e9 : i64 = offsetof(Cpu, whole);
	var e10 : i64 = offsetof(Cpu, lo);
	var e11 : i64 = offsetof(Cpu, hi);
	var e12 : i64 = offsetof(Inner, a);
	var e13 : i64 = offsetof(Inner, b);
	var e14 : i64 = offsetof(Bits, k);

	o.inner.b = 16909060;
	bl.lo = 3;
	c.tag = 1;
	var q : ptr u8 = p as ptr u8;
	var b0 : u8 = @(q + e3);
	var b3 : u8 = @(q + e3 + 3);

	if (t1 != 4) return 1;
	if (t2 != 1) return 2;
	if (t3 != 4) return 3;
	if (t4 != 4) return 4;
	if (t5 != 4) return 5;
	if (t6 != 4) return 6;
	if (t7 != 1) return 7;
	if (t8 != 2) return 8;
	if (t9 != 8) return 9;
	if (t10 != 1) return 10;
	if (t11 != 8) return 11;
	if (t12 != 8) return 12;
	if (t13 != 4) return 13;
	if (t14 != 4) return 14;
	if (t15 != 8) return 15;
	if (t16 != 8) return 16;
	if (t17 != 1) return 17;
	if (t18 != 8) return 18;
	if (t19 != 1) return 19;
	if (t20 != 1) return 20;
	if (t21 != 2) return 21;
	if (t22 != 4) return 22;
	if (t23 != 5) return 23;
	if (t24 != 20) return 24;
	if (t25 != 1) return 25;
	if (t26 != 4) return 26;
	if (t27 != 16) return 27;

	if (e1 != 0) return 30;
	if (e2 != 4) return 31;
	if (e3 != 8) return 32;
	if (e4 != 12) return 33;
	if (e5 != 8) return 34;
	if (e6 != 0) return 35;
	if (e7 != 0) return 36;
	if (e8 != 1) return 37;
	if (e9 != 4) return 38;
	if (e10 != 4) return 39;
	if (e11 != 6) return 40;
	if (e12 != 0) return 41;
	if (e13 != 4) return 42;
	if (e14 != 2) return 43;

	// el offset plegado coincide con la dirección real del campo
	if (b0 != 4) return 50;
	if (b3 != 1) return 51;
	if (bl.lo != 3) return 52;
	if (c.tag != 1) return 53;
	if (@st != 97) return 54;

	return 0;
}
