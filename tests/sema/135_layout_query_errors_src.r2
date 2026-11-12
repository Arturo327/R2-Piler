struct P {
	x : u8 : 3;
	y : i32;
	z : ptr i32;
	w : i32[2];
}

struct Bad {
	a : i32;
	a : i32;
}

fn side () : i32 {
	return 1;
}

fn nothing () {
}

fn main () : i64 {
	var p : P = {};
	var v : i32 = 0;
	var a : i64 = alignof(void);
	var b : i64 = alignof(nothing());
	var c : i64 = alignof(side);
	var d : i64 = alignof(1 + 2);
	var e : i64 = alignof(nope);
	var f : i64 = alignof(v.x);
	var g : i64 = alignof(NoSuchType);
	var h : i64 = alignof(Bad);
	var i : i64 = offsetof(Bad, a);
	var j : i64 = offsetof(i32, x);
	var k : i64 = offsetof(ptr P, x);
	var l : i64 = offsetof(void, x);
	var m : i64 = offsetof(P, nope);
	var n : i64 = offsetof(P, x);
	var o : i64 = offsetof(P, w.x);
	var q : i64 = offsetof(P, z.x);

	return p.y as i64 + v as i64 + a + b + c + d + e + f + g + h + i + j + k + l
			+ m + n + o + q;
}
