fn classify(n : i64) : i64 {
	var r : i64 = 0;

	switch (n) {
	case (1) {
		r = 10;
	}
	case (2) {
		r = 20;
	}
	default {
		r = 30;
	}
	}
	return r;
}

fn main() : i64 {
	return 0;
}