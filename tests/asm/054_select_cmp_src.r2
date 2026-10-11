// OPT: SELECT fused cmp+cmovcc over signed, unsigned and narrow types.
// Each helper's pure comparison evaluates late, so `make_cmp_instr`
// fuses it with the select: `a > b` (i64) -> `cmpq + cmovle`,
// `a < b` (u64) -> `cmpq + cmovae` (unsigned), `a == b` -> `cmovne`,
// narrow `a > b` (i8) -> `cmpb + cmovle`, narrow `a < b` (u16) ->
// `cmpw + cmovae`. O0/O1 materialize the compare (`setcc`) and branch
// (`jcc` + `jmp`). Helpers call `opaque(0)` to block O2 inlining.
// grep O2: `cmovle`, `cmovae`, `cmovne` each present (5 `cmov` total);
// grep O0/O1: no `cmov` at all.
fn opaque(x : i64) : i64 {
	return x;
}
fn pickgt(a : i64, b : i64, x : i64, y : i64) : i64 {
	opaque(0);
	var r : i64 = 0;
	if (a > b) {
		r = x;
	} else {
		r = y;
	}
	return r;
}
fn pickult(a : u64, b : u64, x : u64, y : u64) : u64 {
	opaque(0);
	var r : u64 = 0 as u64;
	if (a < b) {
		r = x;
	} else {
		r = y;
	}
	return r;
}
fn pickeq(a : i64, b : i64, x : i64, y : i64) : i64 {
	opaque(0);
	var r : i64 = 0;
	if (a == b) {
		r = x;
	} else {
		r = y;
	}
	return r;
}
fn pick8(a : i8, b : i8, x : i8, y : i8) : i8 {
	opaque(0);
	var r : i8 = 0 as i8;
	if (a > b) {
		r = x;
	} else {
		r = y;
	}
	return r;
}
fn picku16(a : u16, b : u16, x : u16, y : u16) : u16 {
	opaque(0);
	var r : u16 = 0 as u16;
	if (a < b) {
		r = x;
	} else {
		r = y;
	}
	return r;
}
fn main() : i64 {
	if (pickgt(9, 3, 11, 22) != 11) {
		return 1;
	}
	if (pickgt(3, 9, 11, 22) != 22) {
		return 1;
	}
	if ((pickult(3 as u64, 9 as u64, 30 as u64, 40 as u64) as i64) != 30) {
		return 1;
	}
	if ((pickult(9 as u64, 3 as u64, 30 as u64, 40 as u64) as i64) != 40) {
		return 1;
	}
	if (pickeq(7, 7, 50, 60) != 50) {
		return 1;
	}
	if (pickeq(7, 8, 50, 60) != 60) {
		return 1;
	}
	if ((pick8(5 as i8, 2 as i8, 70 as i8, 80 as i8) as i64) != 70) {
		return 1;
	}
	if ((pick8(2 as i8, 5 as i8, 70 as i8, 80 as i8) as i64) != 80) {
		return 1;
	}
	if ((picku16(10 as u16, 20 as u16, 300 as u16, 400 as u16) as i64) != 300) {
		return 1;
	}
	if ((picku16(20 as u16, 10 as u16, 300 as u16, 400 as u16) as i64) != 400) {
		return 1;
	}
	return 0;
}
