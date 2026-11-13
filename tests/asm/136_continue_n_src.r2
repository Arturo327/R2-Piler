fn main() : i64 {
	var total : i64 = 0;

	for (var i : i64 = 0; i < 4; i++) {
		for (var j : i64 = 0; j < 4; j++) {
			if (j == 2) continue;
			if (j == 3) continue 2;
			total += 10;
		}
		total += 1000;
	}
	return total - 80;
}