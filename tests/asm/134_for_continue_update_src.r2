fn main() : i64 {
	var sum : i64 = 0;

	for (var i : i64 = 0; i < 10; i++) {
		if (i == 5) continue;
		if (i == 8) break;
		sum += i;
	}
	return sum - 23;
}