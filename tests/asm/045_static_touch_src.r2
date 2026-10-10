// OPT: static_init with a call barrier. `g1 = 42` binds to .data (const +
// str before any call/jump); the `pure()` call stops the scan, so `g2 = 99`
// stays dynamic (.bss + runtime store) even though it is a constant.
// grep: `g1` under `.data`, `g2` under `.bss`.
fn pure() : i64 {
	return 7;
}
var g1 : i64 = 42;
var h : i64 = pure();
var g2 : i64 = 99;
fn main() : i64 {
	if (g1 != 42) {
		return 1;
	}
	if (h != 7) {
		return 2;
	}
	if (g2 != 99) {
		return 3;
	}
	return 0;
}
