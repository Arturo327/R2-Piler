fn f() : i64 {
	while (1) {
		break;
	}

	for (var i : i64 = 0; i < 3; i++) {
		if (i == 1) continue;
		if (i == 2) break 2;
		continue 2;
	}
	return 0;
}