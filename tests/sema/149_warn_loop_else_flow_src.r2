fn natural() : i64 {
	var x : i64;
	var c : i64 = 0;

	while (c < 3) {
		c++;
	} else {
		x = 5;
	}
	return x;
}

fn maybe() : i64 {
	var x : i64;
	var c : i64 = 0;

	while (c < 3) {
		c++;
		if (c == 2) break;
	} else {
		x = 5;
	}
	return x;
}

fn main() : i64 {
	return natural() + maybe() - 5;
}