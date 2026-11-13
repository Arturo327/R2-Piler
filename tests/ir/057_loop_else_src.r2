fn main() : i64 {
	var i : i64 = 0;
	var sum : i64 = 0;

	while (i < 3) {
		i++;
	} else {
		sum += 10;
	}

	for (var j : i64 = 0; j < 3; j++) {
		if (j == 1) continue;
	} else {
		sum += j;
	}

	for (var a : i64 = 0; a < 2; a++) {
		for (var b : i64 = 0; b < 2; b++) {
			if (b == 1) continue 2;
		} else {
			sum += 1;
		}
	} else {
		sum += a;
	}
	return sum;
}