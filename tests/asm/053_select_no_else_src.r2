// OPT: SELECT without else (else keeps the previous value).
// `keep` (`if (c) r = 10;`, cond variable) lowers to `testq + cmove`
// with the else arm wired to the incoming `r`; `keepc` (`if (a == b)`)
// fuses to `cmpq + cmovne`. O0/O1 keep the branch (`je` + `jmp`).
// Helpers call `opaque(0)` so O2 cannot inline them (const args would
// fold the select away); main checks kept vs overwritten values.
// grep O2: `cmove` (keep) and `cmovne` (keepc) present;
// grep O0/O1: no `cmov` at all.
fn opaque(x : i64) : i64 {
	return x;
}
fn keep(c : i64, v : i64) : i64 {
	opaque(0);
	var r : i64 = v;
	if (c) {
		r = 10;
	}
	return r;
}
fn keepc(a : i64, b : i64, v : i64) : i64 {
	opaque(0);
	var r : i64 = v;
	if (a == b) {
		r = 77;
	}
	return r;
}
fn main() : i64 {
	if (keep(1, 5) != 10) {
		return 1;
	}
	if (keep(0, 5) != 5) {
		return 1;
	}
	if (keep(0, -3) != -3) {
		return 1;
	}
	if (keep(1, -3) != 10) {
		return 1;
	}
	if (keepc(4, 4, 9) != 77) {
		return 1;
	}
	if (keepc(4, 5, 9) != 9) {
		return 1;
	}
	if (keepc(0, 1, -8) != -8) {
		return 1;
	}
	if (keepc(5, 5, 0) != 77) {
		return 1;
	}
	return 0;
}
