fn main() : i64 {
	var i : i64 = 0;
	var sum : i64 = 0;

	while (i < 10) {
		i++;
		if (i == 3) continue;
		if (i == 7) break;
		sum += i;
	}
	return sum - 18;
}