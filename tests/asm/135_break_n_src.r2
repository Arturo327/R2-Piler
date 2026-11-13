fn main() : i64 {
	var total : i64 = 0;

	for (var i : i64 = 0; i < 4; i++) {
		for (var j : i64 = 0; j < 4; j++) {
			if (i == 2 && j == 2) break 2;
			total += 1;
		}
	}
	return total - 10;
}