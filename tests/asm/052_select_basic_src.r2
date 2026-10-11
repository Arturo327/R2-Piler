// OPT: SELECT basic (generic + fused) on straight-line calls.
// `pickv` (cond variable) lowers to `testq %rdx + cmove`;
// `pickc` (`a < b`) fuses to `cmpq + cmovge`; `pickm` mixes a local arm
// with a literal arm (`else 99`). O0/O1 emit classic branches
// (`cmp $0 + je` / `cmp + jge` + `jmp` + labels) instead.
// Each helper calls `opaque(0)` so O2 cannot inline it away (else the
// const call args would fold the select into a MOVE and the `cmov`
// would vanish); main only checks results.
// grep O2: `cmove` (pickv/pickm) and `cmovge` (pickc) present;
// grep O0/O1: no `cmov` at all.
fn opaque(x : i64) : i64 {
	return x;
}
fn pickv(c : i64, a : i64, b : i64) : i64 {
	opaque(0);
	var r : i64 = 0;
	if (c) {
		r = a;
	} else {
		r = b;
	}
	return r;
}
fn pickc(a : i64, b : i64, x : i64, y : i64) : i64 {
	opaque(0);
	var r : i64 = 0;
	if (a < b) {
		r = x;
	} else {
		r = y;
	}
	return r;
}
fn pickm(c : i64, a : i64) : i64 {
	opaque(0);
	var r : i64 = 0;
	if (c) {
		r = a;
	} else {
		r = 99;
	}
	return r;
}
fn main() : i64 {
	if (pickv(1, 10, 20) != 10) {
		return 1;
	}
	if (pickv(0, 10, 20) != 20) {
		return 1;
	}
	if (pickv(7, -5, 5) != -5) {
		return 1;
	}
	if (pickv(0, -5, 5) != 5) {
		return 1;
	}
	if (pickc(3, 9, 100, 200) != 100) {
		return 1;
	}
	if (pickc(9, 3, 100, 200) != 200) {
		return 1;
	}
	if (pickm(1, 42) != 42) {
		return 1;
	}
	if (pickm(0, 42) != 99) {
		return 1;
	}
	return 0;
}
