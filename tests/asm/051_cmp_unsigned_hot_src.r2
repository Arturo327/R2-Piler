// OPT: unsigned compare + branch fusion (cmp+jb/jae) on the hot path.
// Both the loop bound (`j < 10000000`) and the body (`j < 5000000`) fuse
// LT+JZ into `cmpq + jae`, with `setcc` using the unsigned condition.
// cnt counts j in 0..4999999 = 5000000.
fn main() : i64 {
	var cnt : u64 = 0;
	for (var j : u64 = 0; j < 10000000; j = j + 1) {
		if (j < 5000000) {
			cnt = cnt + 1;
		}
	}
	if (cnt == 5000000) {
		return 0;
	}
	return 1;
}
