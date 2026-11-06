var g : i64 = 0;

fn bump(x : i64) : i64 {
	g = g + 1;
	return x;
}

fn pick(c : i64, a : i64, b : i64) : i64 {
	return c ? a : b;
}

fn main() : i64 {
	var t : i64 = 1;
	var f : i64 = 0;
	var a : i64 = 10;
	var b : i64 = 20;
	var c : i64 = a > b ? a : b;

	if (c != 20) { return 1; }
	if ((t ? bump(5) : bump(7)) != 5) { return 2; }
	if (g != 1) { return 3; }
	if ((f ? bump(5) : bump(7)) != 7) { return 4; }
	if (g != 2) { return 5; }

	var p : ptr i64 = $a;
	var q : ptr i64 = t ? p : $b;

	if (@q != 10) { return 6; }

	var x : i8 = a > b ? 1 : 2;
	if (x != 2) { return 7; }

	var n : i64 = a < b ? (b > 15 ? 30 : 40) : 50;
	if (n != 30) { return 8; }

	return pick(t, 3, 4) + pick(f, 3, 4) - 7;
}