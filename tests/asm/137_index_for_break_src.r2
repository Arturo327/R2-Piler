fn main() : i64 {
	arr m : i64[2][3] = {{1, 2, 3}, {4, 5, 6}};
	var outer : i64 = 0;

	for (var i, j : m) {
		outer += 1;
		outer += i;
		outer += j;
		break;
	}
	return outer - 3;
}