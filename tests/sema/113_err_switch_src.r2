fn f() : i64 {
	return 1;
}

fn main() : i64 {
	var n : i64 = 1;
	var p : ptr i64 = $n;

	switch (n) {
	case (1, 1) {
	}
	}

	switch (n) {
	case (f()) {
	}
	default {
	}
	}

	switch (p) {
	case (1) {
	}
	default {
	}
	}

	switch (n) {
	case () {
	}
	default {
	}
	}

	switch (n) {
	case (1) {
		var x : i64 = 2;
	}
	case (2) {
		var y : i64 = x;
	}
	default {
	}
	}

	switch ("abc") {
	default {
	}
	}
	return 0;
}