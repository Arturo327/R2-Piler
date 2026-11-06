var g : i64 = 5;

enum Color : i64 {
	Red = 1,
	Blue = 2,
}

fn choose(c : i64, a : i64, b : i64) : i64 {
	var n : i8 = c ? 1 : 2;
	var w : i64 = c ? a : 3;
	var p : ptr i64 = c ? $g : NULL;
	var s : ptr i8 = c ? "yes" : "no";
	var e : Color = c ? Color.Red : Color.Blue;
	var q : i64 = c ? a : b;

	if (p == NULL) { return 0; }
	if (s == NULL) { return 1; }
	return q + w + n + (e as i64);
}

fn main() : i64 {
	var p : ptr i64 = $g;
	var q : ptr i64 = 1 ? p : $g;

	if (q == NULL) { return 1; }
	return choose(1, 2, 3) + @q;
}