// OPT: constant immediates + RMW in memory (no slot, no load) hot.
// `s + 5` -> `addq $5, -8(%rsp)`, `s & 255` -> `andq $255, -8(%rsp)`,
// `i + 1` -> `addq $1, -16(%rsp)` (RMW via make_rmw, better than op_rax);
// single-CONST regs have no stack home.
// Without it each constant would be loaded from its slot via `movq mem, %rcx`.
// grep: `addq $5, -` and `andq $255, -` present, no `movq mem, %rcx` for consts.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 10000000; i = i + 1) {
		s = s + 5;
		s = s & 255;
	}
	if (s == 128) {
		return 0;
	}
	return 1;
}
