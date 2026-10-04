// OPT: MUL strength reduction (shl/lea vs imul) on the hot path.
// Every iteration does `i * 8` (-> shlq $3) and `i * 5` (-> leaq).
// Without it both would be `imulq $imm, %rax` (~3 cycles vs 1).
// grep: `shlq $3` and `leaq (%rax,%rax,4)` present, no `imulq` in loop.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		s = s + i * 8 + i * 5;
	}
	if (s == 415999948000000) {
		return 0;
	}
	return 1;
}
