// OPT: CMP+branch fusion (cmp+jcc vs sete/store/reload) on the hot path.
// Both the loop condition (`i < 10000000`) and the body (`i < 5000000`)
// fuse EQ/LT+JZ into a single `cmpq $imm, %rax` + `jge`.
// Without fusion each would materialize 0/1 via `setcc; movzbl; movq` + reload.
// grep: `cmpq $5000000, %rax` + `jge` present, no `sete` in loop.
fn main() : i64 {
	var cnt : i64 = 0;
	for (var i : i64 = 0; i < 10000000; i = i + 1) {
		if (i < 5000000) {
			cnt = cnt + 1;
		}
	}
	if (cnt == 5000000) {
		return 0;
	}
	return 1;
}
