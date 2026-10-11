// OPT: SELECT in a small loop body (cmov vs branches per iteration).
// Each of the 800 iterations picks `v = 3` for odd `i` else `v = 7`
// via `(i & 1) != 0`; O2 fuses it to `andq + cmpq $0 + cmove` with no
// branch in the body, while O0/O1 emit `cmpq $0 + je + jmp` per iter.
// s = 400 odd * 3 + 400 even * 7 = 1200 + 2800 = 4000, so O2 measures
// fewer Ir than O0/O1 with the same exit code.
// grep O2 loop body: `cmove` present, no `je .L` between `andq` and `addq`;
// grep O0/O1 loop body: `je` + `jmp` present, no `cmov`.
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 800; i = i + 1) {
		var v : i64 = 0;
		if ((i & 1) != 0) {
			v = 3;
		} else {
			v = 7;
		}
		s = s + v;
	}
	if (s == 4000) {
		return 0;
	}
	return 1;
}
