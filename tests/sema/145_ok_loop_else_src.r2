fn main() : i64 {
	var i : i64 = 0;
	var a : i64 = 0;

	while (i < 3) {
		i++;
	} else {
		a = 1;
	}

	for (var j : i64 = 0; j < 3; j++) {
		if (j == 1) continue;
	} else {
		a += j;
	}
	return a;
}