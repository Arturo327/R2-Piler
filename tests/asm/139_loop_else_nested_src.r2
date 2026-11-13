fn main() : i64 {
	var inner : i64 = 0;
	var outer : i64 = 0;

	for (var i : i64 = 0; i < 3; i++) {
		for (var j : i64 = 0; j < 3; j++) {
			if (j == 1) continue 2;
		} else {
			inner += 1;
		}
	} else {
		outer = 1;
	}

	var s : i64 = 0;
	var t : i64 = 0;

	for (var a : i64 = 0; a < 3; a++) {
		for (var b : i64 = 0; b < 3; b++) {
			if (a == 1 && b == 1) break 2;
		} else {
			s += 1;
		}
	} else {
		t = 1;
	}

	return (inner == 0 && outer == 1 && s == 1 && t == 0) ? 0 : 1;
}