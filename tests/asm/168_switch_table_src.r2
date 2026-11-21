// Dense switch dispatch: 16 contiguous case values.
// A linear EQ/JNZ chain is O(n) per call; this fixture drives the
// jump-table lowering and checks that dispatch gets close to O(1).
fn dense(n : i64) : i64 {
	switch (n) {
	case (0) { return 100; }
	case (1) { return 101; }
	case (2) { return 102; }
	case (3) { return 103; }
	case (4) { return 104; }
	case (5) { return 105; }
	case (6) { return 106; }
	case (7) { return 107; }
	case (8) { return 108; }
	case (9) { return 109; }
	case (10) { return 110; }
	case (11) { return 111; }
	case (12) { return 112; }
	case (13) { return 113; }
	case (14) { return 114; }
	case (15) { return 115; }
	default { return -1; }
	}
}

pub fn main() : i64 {
	var i : i64 = 0;
	while (i < 1000) {
		var n : i64 = i & 15;
		if (dense(n) != 100 + n) { return 1; }
		i = i + 1;
	}
	if (dense(99) != -1) { return 2; }
	if (dense(-7) != -1) { return 3; }
	return 0;
}
