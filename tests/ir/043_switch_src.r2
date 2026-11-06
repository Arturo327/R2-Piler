fn classify(n : i64) : i64 {
	switch (n) {
	case (1, 2) {
		return 10;
	}
	case (3) {
		return 20;
	}
	default {
		return 30;
	}
	}
}

fn main() : i64 {
	return classify(3);
}