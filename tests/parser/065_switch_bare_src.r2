fn main() : i64 {
	var n : i64 = 1;

	switch (n) {
	case (1)
		n = 2;
	case (2, 3) {
		n = 3;
	}
	case (4)
		var x : i64 = 9;
	default
		n = 4;
	}
	return n;
}