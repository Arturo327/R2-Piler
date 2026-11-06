enum E : i64 {
	A = 1,
	B = 2,
}

fn main() : i64 {
	var d : i64 = 1;

	switch (d) {
	case (1, 2) {
		var x : i64 = 3;
	}
	case (E.A) {
	}
	default {
	}
	}
	return 0;
}