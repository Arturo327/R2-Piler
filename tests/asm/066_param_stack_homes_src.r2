// M3: 16 parameters, so targets 6..15 arrive on the stack. Every parameter is
// used twice, which gives all of them a home score: the colorer homes 14 and
// spills two, so the stack PARAM group needs a scratch for the spilled ones
// while the first sixteen PARAM dsts are live. main's ARG group has ten stack
// arguments and must keep its sources intact across the parallel move. wide
// exceeds the inliner budget (INLINE_MAX_INSTRS), so the groups are emitted.
// wide(1..16) = 2 * (1 + ... + 16) = 272.
fn wide(p0 : i64, p1 : i64, p2 : i64, p3 : i64, p4 : i64, p5 : i64,
		p6 : i64, p7 : i64, p8 : i64, p9 : i64, p10 : i64, p11 : i64,
		p12 : i64, p13 : i64, p14 : i64, p15 : i64) : i64 {
	return p0 + p0 + p1 + p1 + p2 + p2 + p3 + p3 + p4 + p4
		+ p5 + p5 + p6 + p6 + p7 + p7 + p8 + p8 + p9 + p9
		+ p10 + p10 + p11 + p11 + p12 + p12 + p13 + p13
		+ p14 + p14 + p15 + p15;
}

fn main() : i64 {
	if (wide(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16) == 272) {
		return 0;
	}
	return 1;
}
