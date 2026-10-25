var g1 : i64 = 1;
var g2 : i64 = 2;
var g3 : i64 = 3;
var g4 : i64 = 4;
var g5 : i64 = 5;
var g6 : i64 = 6;
var g7 : i64 = 7;
var g8 : i64 = 8;
var g9 : i64 = 9;
var g10 : i64 = 10;
var g11 : i64 = 11;
var g12 : i64 = 12;
var g13 : i64 = 13;
var g14 : i64 = 14;
var g15 : i64 = 15;
var g16 : i64 = 16;
var g17 : i64 = 17;
var g18 : i64 = 18;

fn main () : i64
{
	var a : i64 = g1;
	var b : i64 = g2;
	var c : i64 = g3;
	var d : i64 = g4;
	var e : i64 = g5;
	var f : i64 = g6;
	var g : i64 = g7;
	var h : i64 = g8;
	var i : i64 = g9;
	var j : i64 = g10;
	var k : i64 = g11;
	var l : i64 = g12;
	var m : i64 = g13;
	var n : i64 = g14;
	var o : i64 = g15;
	var p : i64 = g16;
	var q : i64 = g17;
	var r : i64 = g18;

	var t1 : i64 = a + 5000000000 - 5000000000;
	var t2 : i64 = r * 7;
	var t3 : i64 = r / 10 + r % 10;
	var t4 : i64 = q / c + q % c;
	var t5 : i64 = (p << c) - (p >> c);
	var t6 : i64 = (a < 5000000000) + (r > 4294967296);

	return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r
			+ t1 + t2 + t3 + t4 + t5 + t6;
}
