fn main() : i64 {
	var i : i64 = 0;
	var a : i64 = 0;
	var b : i64 = 0;
	var c : i64 = 0;

	while (i < 3) {
		i++;
	} else {
		a = 1;
	}

	for (var j : i64 = 0; j < 3; j++) {
		if (j == 1) continue;
	} else {
		b = 1;
	}

	var k : i64 = 0;

	while (k < 5) {
		k++;
		if (k == 2) break;
	} else {
		c = 1;
	}
	return (a == 1 && b == 1 && c == 0) ? 0 : 1;
}