// Sparse switch: 16 case values spaced 1000 apart. Too sparse for a
// jump table, so this drives the balanced comparison-tree lowering.
fn sparse(n : i64) : i64 {
	switch (n) {
	case (1000) { return 1; }
	case (2000) { return 2; }
	case (3000) { return 3; }
	case (4000) { return 4; }
	case (5000) { return 5; }
	case (6000) { return 6; }
	case (7000) { return 7; }
	case (8000) { return 8; }
	case (9000) { return 9; }
	case (10000) { return 10; }
	case (11000) { return 11; }
	case (12000) { return 12; }
	case (13000) { return 13; }
	case (14000) { return 14; }
	case (15000) { return 15; }
	case (16000) { return 16; }
	default { return -1; }
	}
}

pub fn main() : i64 {
	var i : i64 = 0;
	while (i < 1000) {
		var k : i64 = i & 15;
		if (sparse(1000 * (k + 1)) != k + 1) { return 1; }
		i = i + 1;
	}
	if (sparse(7) != -1) { return 2; }
	if (sparse(999999) != -1) { return 3; }
	return 0;
}
