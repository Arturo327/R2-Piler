// OPT: unsigned compare + branch fusion (cmp+jb/jae) on the hot path.
// Both the loop bound (`j < 1000`) and the body (`j < 500`) fuse
// LT+JZ into `cmpq + jae`, with `setcc` using the unsigned condition.
// cnt counts j in 0..499 = 500.
fn main() : i64 {
	var cnt : u64 = 0;
	for (var j : u64 = 0; j < 1000; j = j + 1) {
		if (j < 500) {
			cnt = cnt + 1;
		}
	}
	if (cnt == 500) {
		return 0;
	}
	return 1;
}
