fn h(q : ptr i64) : i64 {
	return @q;
}

fn readg() : i64 {
	return @$g3;
}

var p : ptr i64 = $g1;
var x1 : i64 = @p;
var x2 : i64 = h($g2);
var x3 : i64 = readg();
var g1 : i64 = 5;
var g2 : i64 = 6;
var g3 : i64 = 7;

fn main() : i64 {
	if (x1 != 0) { return 1; }
	if (x2 != 0) { return 2; }
	if (x3 != 0) { return 3; }
	if (@p != 5) { return 4; }
	if (@$g2 != 6) { return 5; }
	if (@$g3 != 7) { return 6; }
	return 0;
}
