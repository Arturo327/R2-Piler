fn main() : i64 {
	var i : i64 = 0;
	var sum : i64 = 0;

	while (i < 5) {
		i++;
		if (i == 2) continue;
		if (i == 4) break;
		sum += i;
	}

	for (var j : i64 = 0; j < 3; j++) {
		if (j == 1) continue;
		sum += j;
	}

	for (var a : i64 = 0; a < 2; a++) {
		for (var b : i64 = 0; b < 2; b++) {
			if (b == 1) continue 2;
			if (a == 1) break 2;
			sum += a;
		}
	}
	return sum;
}