fn main() : i64 {
	var i : i64;
	var j : i64;

	for (i, j; i < 3; i++) { j = j + i; }
	return j;
}
