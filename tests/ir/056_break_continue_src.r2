fn main() : i64 {
	var i : i64 = 0;
	var sum : i64 = 0;

	while (i < 10) {
		i++;
		if (i == 3) continue;
		if (i == 7) break;
		sum += i;
	}

	for (var j : i64 = 0; j < 10; j++) {
		if (j == 5) continue;
		if (j == 8) break;
		sum += j;
	}

	for (var a : i64 = 0; a < 3; a++) {
		for (var b : i64 = 0; b < 3; b++) {
			if (b == 1) continue 2;
			if (a == 2) break 2;
			sum += b;
		}
	}
	return sum;
}