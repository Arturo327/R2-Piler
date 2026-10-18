// M1: in-place RMW on callee-saved register homes must keep the canonical
// image for signed narrow types: `addb $1, %bl` + `movsbq %bl, <home>`
// (rmw_sext repair). 120 + 300 wraps through 127 down to -92 in i8.
fn main() : i64 {
	var c : i8 = 120;
	for (var i : i64 = 0; i < 300; i = i + 1) {
		c = c + 1;
	}
	if (c == -92) {
		return 0;
	}
	return 1;
}
