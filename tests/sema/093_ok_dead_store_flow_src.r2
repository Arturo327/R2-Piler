fn main() : i64 {
	var a : i64 = 1;
	var s : i64 = a;
	a = 2;
	var c : i64 = 3;
	if (a > 0) { c = 4; }
	var d : i64 = 5;
	var i : i64 = 0;
	while (i < 3) { d = d + 1; i += 1; }
	var e : i64 = 6;
	var p : ptr i64 = $e;
	e = 7;
	return s + c + d + @p + a + i;
}
