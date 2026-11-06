fn main() : i64 {
	var a : i64 = 1;
	var b : i64 = a ? 2 : 3;
	var c : i64 = a = b ? 4 : 5;
	var d : i64 = a ? b : a ? 1 : 2;
	var e : i64 = (a + 1) * (b > 0 ? 2 : 3);
	var f : i64 = a || b ? 1 : 2;
	var g : i64 = a ? 1 : b = 2;
	return 0;
}