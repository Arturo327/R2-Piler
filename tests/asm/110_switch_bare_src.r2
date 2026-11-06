fn classify(n : i64) : i64 {
	switch (n) {
	case (1)
		return 10;
	case (2) {
		return 20;
	}
	default
		return 30;
	}
}

fn main() : i64 {
	if (classify(1) != 10) { return 1; }
	if (classify(2) != 20) { return 2; }
	if (classify(3) != 30) { return 3; }

	var r : i64 = 0;
	var n : i64 = 2;

	switch (n) {
	case (1)
		r = 1;
	case (2)
		r = 2;
	case (3, 4) {
		var t : i64 = 5;
		r = t;
	}
	default
		r = 9;
	}
	if (r != 2) { return 4; }

	n = 3;

	switch (n) {
	case (1) r = 10;
	case (2) r = 30;
	default r = 40;
	}
	if (r != 40) { return 5; }

	switch (n) {
	case (3) if (r == 40) r = 70; else r = 71;
	default r = 0;
	}
	if (r != 70) { return 6; }

	switch (n) {
	case (4)
		r = 80;
	default
		if (r == 70) r = 90; else r = 91;
	}
	if (r != 90) { return 7; }

	return 0;
}