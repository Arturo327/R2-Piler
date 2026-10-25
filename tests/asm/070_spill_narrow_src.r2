var n1 : i8 = 1;
var n2 : i8 = 2;
var n3 : i8 = 3;
var n4 : u8 = 4;
var n5 : u8 = 5;
var n6 : u8 = 6;
var n7 : i16 = 7;
var n8 : i16 = 8;
var n9 : i16 = 9;
var n10 : u16 = 10;
var n11 : u16 = 11;
var n12 : u16 = 12;
var n13 : i32 = 13;
var n14 : i32 = 14;
var n15 : i32 = 15;
var n16 : u32 = 16;
var n17 : u32 = 17;
var n18 : u32 = 18;

fn main () : i64
{
	var a1 : i8 = n1;
	var a2 : i8 = n2;
	var a3 : i8 = n3;
	var a4 : u8 = n4;
	var a5 : u8 = n5;
	var a6 : u8 = n6;
	var a7 : i16 = n7;
	var a8 : i16 = n8;
	var a9 : i16 = n9;
	var a10 : u16 = n10;
	var a11 : u16 = n11;
	var a12 : u16 = n12;
	var a13 : i32 = n13;
	var a14 : i32 = n14;
	var a15 : i32 = n15;
	var a16 : u32 = n16;
	var a17 : u32 = n17;
	var a18 : u32 = n18;

	var s1 : i8 = a1 + a2 + a3;
	var s2 : u8 = a4 + a5 + a6;
	var s3 : i16 = a7 + a8 + a9;
	var s4 : u16 = a10 + a11 + a12;
	var s5 : i32 = a13 + a14 + a15;
	var s6 : u32 = a16 + a17 + a18;

	var t : i64 = 0;
	t = t + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9;
	t = t + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18;
	t = t + s1 + s2 + s3 + s4 + s5 + s6;
	return t;
}
