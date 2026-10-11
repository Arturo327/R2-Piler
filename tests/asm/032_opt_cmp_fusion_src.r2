// OPT: CMP+branch fusion (cmp+jcc vs sete/store/reload) on the hot path.
// Both the loop condition (`i < 1000`) and the body (`i < 500`)
// fuse EQ/LT+JZ into a single `cmpq $imm, %rax` + `jge`.
// Without fusion each would materialize 0/1 via `setcc; movzbl; movq` + reload.
// grep: `cmpq $500, %rax` + `jge` present, no `sete` in loop.
fn main() : i64 {
	var cnt : i64 = 0;
	for (var i : i64 = 0; i < 1000; i = i + 1) {
		if (i < 500) {
			cnt = cnt + 1;
		}
	}
	if (cnt == 500) {
		return 0;
	}
	return 1;
}
