struct P {
	x : i32;
	y : i32;
	z : i8;
}

struct Q {
	p : P;
	n : i32;
}

struct WithArr {
	a : i32[3];
	k : i32;
}

struct Bits {
	lo : u3;
	hi : u5;
}

union V {
	struct { a : u8; b : u8; };
	c : u16;
}

union Wide {
	whole : i64;
	bytes : u8[8];
}

packed struct PB {
	tag : u8;
	w : u32;
	bits : Bits;
}

packed struct Big {
	head : i64;
	body : u8[61];
	tail : i32;
}

var g : P = {.y = 21};
var gq : Q = {.p = {.z = 5}};
var gv : V = {};

fn main () : i64 {
	var a : P = {};
	var b : P = {.x = 1};
	var c : P = {.x = 2, .y = 3, .z = 4};
	var q : Q = {.p = {.x = 6}};
	var w : WithArr = {.k = 7};
	var w2 : WithArr = {.a = {1, 2, 3}};
	var bits : Bits = {.hi = 9};
	var v : V = {.a = 11};
	var v2 : V = {};
	var wide : Wide = {.whole = -1};
	var pb : PB = {.w = 13};
	var big : Big = {.tail = 14};
	static var sp : P = {.z = 15};
	arr arr_p : P[3] = {{.x = 16}, {}, {.y = 18}};
	var bp : ptr u8 = $bits as ptr u8;
	var v2p : ptr u8 = $v2 as ptr u8;
	var pp : ptr u8 = $pb as ptr u8;
	var gp : ptr u8 = $big as ptr u8;
	var sp2 : ptr u8 = $sp as ptr u8;
	var sz : i64 = sizeof(P) as i64;

	if (sz != 12) return 1;
	sz = sizeof(Q) as i64;
	if (sz != 16) return 2;
	sz = sizeof(WithArr) as i64;
	if (sz != 16) return 3;
	sz = sizeof(V) as i64;
	if (sz != 2) return 4;
	sz = sizeof(PB) as i64;
	if (sz != 6) return 5;
	sz = sizeof(Big) as i64;
	if (sz != 73) return 6;

	if (a.x != 0) return 7;
	if (a.y != 0) return 8;
	if (a.z as i64 != 0) return 9;
	if (b.x != 1) return 10;
	if (b.y != 0) return 11;
	if (c.x != 2) return 12;
	if (c.y != 3) return 13;
	if (c.z as i64 != 4) return 14;

	if (q.p.x != 6) return 15;
	if (q.p.y != 0) return 16;
	if (q.p.z as i64 != 0) return 17;
	if (q.n != 0) return 18;

	if (w.k != 7) return 19;
	if (w.a[0] != 0) return 20;
	if (w.a[2] != 0) return 21;
	if (w2.k != 0) return 22;
	if (w2.a[2] != 3) return 23;

	if (bits.hi as i64 != 9) return 24;
	if (bits.lo as i64 != 0) return 25;
	if (@bp as i64 != 72) return 26;

	if (v.a as i64 != 11) return 27;
	if (v.b as i64 != 0) return 28;
	if (v.c != 11) return 29;
	if (v2.c != 0) return 30;
	if (@v2p as i64 != 0) return 31;
	if (@(v2p + 1) as i64 != 0) return 32;
	if (wide.whole != -1) return 33;
	if (wide.bytes[7] as i64 != 255) return 34;

	if (pb.tag as i64 != 0) return 35;
	if (pb.w != 13) return 36;
	if (pb.bits.lo as i64 != 0) return 37;
	if (pb.bits.hi as i64 != 0) return 38;
	if (@pp as i64 != 0) return 39;
	if (@(pp + 1) as i64 != 13) return 40;
	if (@(pp + 5) as i64 != 0) return 41;

	if (big.head != 0) return 42;
	if (big.body[0] as i64 != 0) return 43;
	if (big.body[60] as i64 != 0) return 44;
	if (big.tail != 14) return 45;
	if (@gp as i64 != 0) return 46;
	if (@(gp + 72) as i64 != 0) return 47;

	if (g.x != 0) return 48;
	if (g.y != 21) return 49;
	if (gq.p.x != 0) return 50;
	if (gq.p.z as i64 != 5) return 51;
	if (gq.n != 0) return 52;
	if (gv.c != 0) return 53;

	if (sp.x != 0) return 54;
	if (sp.z as i64 != 15) return 55;
	if (@(sp2 + 4) as i64 != 0) return 56;
	sp.x = 9;
	if (sp.z as i64 != 15) return 57;

	if (arr_p[0].x != 16) return 58;
	if (arr_p[0].y != 0) return 59;
	if (arr_p[1].x != 0) return 60;
	if (arr_p[1].y != 0) return 61;
	if (arr_p[2].x != 0) return 62;
	if (arr_p[2].y != 18) return 63;

	return 0;
}