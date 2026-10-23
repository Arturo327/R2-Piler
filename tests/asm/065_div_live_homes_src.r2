// M3: fixed registers across DIV/MOD and a variable shift. x/y/z/w stay live
// across the whole loop body, so the repair pass must spill any of them that
// coloring put in %rax/%rdx (DIV/MOD clobbers) or %rcx (variable shift count).
// The loop also keeps the divisor y live across the DIV and MOD, which forces
// the backend to load the divisor into scratch instead of assuming %rdx.
// mix(17, 3, 9): x=28, y=10, z=23, w=19, each iteration adds
// 2 + 3 + (19 << 10) + 28 + 10 + 23 + 19 = 19541, then 9 * 19541 + 80 = 175949.
fn mix(a : i64, b : i64, n : i64) : i64 {
	var x : i64 = a + 11;
	var y : i64 = b + 7;
	var z : i64 = a + b + 3;
	var w : i64 = a - b + 5;
	var s : i64 = 0;
	var i : i64 = 0;
	while (i < n) {
		var q : i64 = x / y;
		var r : i64 = z % y;
		var k : i64 = w << y;
		s = s + q + r + k + x + y + z + w;
		i = i + 1;
	}
	return s + x + y + z + w;
}

fn main() : i64 {
	if (mix(17, 3, 9) == 175949) {
		return 0;
	}
	return 1;
}
