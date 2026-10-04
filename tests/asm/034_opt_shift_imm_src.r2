// OPT: shift-by-constant immediate (shl $imm vs %cl) hot.
// `s << 3` -> `shlq $3, %rax`, `i >> 2` -> `sarq $2, %rax` (signed i64).
// Without it the count would be loaded into `%rcx` + `shlq %cl, %rax`.
// grep: `shlq $3, %rax` and `sarq $2, %rax` present, no `%cl` in loop.
fn main() : i64 {
	var s : i64 = 1;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		s = (s << 3) + (i >> 2);
		s = s & 1048575;
	}
	if (s == 763447) {
		return 0;
	}
	return 1;
}
