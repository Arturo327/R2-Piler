// Wide dense switch: 64 contiguous values. Exercises the jump-table
// lowering at a size where a linear EQ/JNZ chain is O(32) per call.
fn wide(n : i64) : i64 {
	switch (n) {
	case (0) { return 1000; }
	case (1) { return 1001; }
	case (2) { return 1002; }
	case (3) { return 1003; }
	case (4) { return 1004; }
	case (5) { return 1005; }
	case (6) { return 1006; }
	case (7) { return 1007; }
	case (8) { return 1008; }
	case (9) { return 1009; }
	case (10) { return 1010; }
	case (11) { return 1011; }
	case (12) { return 1012; }
	case (13) { return 1013; }
	case (14) { return 1014; }
	case (15) { return 1015; }
	case (16) { return 1016; }
	case (17) { return 1017; }
	case (18) { return 1018; }
	case (19) { return 1019; }
	case (20) { return 1020; }
	case (21) { return 1021; }
	case (22) { return 1022; }
	case (23) { return 1023; }
	case (24) { return 1024; }
	case (25) { return 1025; }
	case (26) { return 1026; }
	case (27) { return 1027; }
	case (28) { return 1028; }
	case (29) { return 1029; }
	case (30) { return 1030; }
	case (31) { return 1031; }
	case (32) { return 1032; }
	case (33) { return 1033; }
	case (34) { return 1034; }
	case (35) { return 1035; }
	case (36) { return 1036; }
	case (37) { return 1037; }
	case (38) { return 1038; }
	case (39) { return 1039; }
	case (40) { return 1040; }
	case (41) { return 1041; }
	case (42) { return 1042; }
	case (43) { return 1043; }
	case (44) { return 1044; }
	case (45) { return 1045; }
	case (46) { return 1046; }
	case (47) { return 1047; }
	case (48) { return 1048; }
	case (49) { return 1049; }
	case (50) { return 1050; }
	case (51) { return 1051; }
	case (52) { return 1052; }
	case (53) { return 1053; }
	case (54) { return 1054; }
	case (55) { return 1055; }
	case (56) { return 1056; }
	case (57) { return 1057; }
	case (58) { return 1058; }
	case (59) { return 1059; }
	case (60) { return 1060; }
	case (61) { return 1061; }
	case (62) { return 1062; }
	case (63) { return 1063; }
	default { return -1; }
	}
}

pub fn main() : i64 {
	var i : i64 = 0;
	while (i < 1000) {
		var n : i64 = i & 63;
		if (wide(n) != 1000 + n) { return 1; }
		i = i + 1;
	}
	if (wide(200) != -1) { return 2; }
	return 0;
}
