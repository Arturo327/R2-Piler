// M2 caller-saved homes: `i`, `acc` and `n` are live across the `step` call
// and keep callee-saved homes; the `i + 1` argument temp and the call result
// are not live across any call, so they take caller-saved homes (%r10/%r11)
// and cost no prologue save (old compiler: 5 callee saves, now 4).
// sum of 3 * (i + 1) + 1 for i = 0..19 is 650.
fn step(x : i64) : i64 {
	return x * 3 + 1;
}
fn chain(n : i64) : i64 {
	var i : i64 = 0;
	var acc : i64 = 0;
	while (i < n) {
		acc = acc + step(i + 1);
		i = i + 1;
	}
	return acc;
}
fn main() : i64 {
	if (chain(20) != 650) {
		return 1;
	}
	return 0;
}
