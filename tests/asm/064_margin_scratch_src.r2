// M3: scratch margin. Fourteen variables are live across `t = v0 + v1`, so
// after coloring every register is occupied by a live home. `t` itself has
// only two uses and stays spilled, so without SCRATCH_MIN the backend has no
// register to materialize the add (`no scratch register available`). The
// regalloc repair pass must spill caller-saved homes until two scratch
// registers are free; the value stays correct because the spilled homes are
// reloaded from their slots.
// squeeze() = 2 * (11 + 12 + ... + 24) + (11 + 12) = 490 + 23 = 513.
var g0 : i64 = 1;
var g1 : i64 = 2;
var g2 : i64 = 3;
var g3 : i64 = 4;
var g4 : i64 = 5;
var g5 : i64 = 6;
var g6 : i64 = 7;
var g7 : i64 = 8;
var g8 : i64 = 9;
var g9 : i64 = 10;
var g10 : i64 = 11;
var g11 : i64 = 12;
var g12 : i64 = 13;
var g13 : i64 = 14;

fn squeeze() : i64 {
	var v0 : i64 = g0 + 10;
	var v1 : i64 = g1 + 10;
	var v2 : i64 = g2 + 10;
	var v3 : i64 = g3 + 10;
	var v4 : i64 = g4 + 10;
	var v5 : i64 = g5 + 10;
	var v6 : i64 = g6 + 10;
	var v7 : i64 = g7 + 10;
	var v8 : i64 = g8 + 10;
	var v9 : i64 = g9 + 10;
	var v10 : i64 = g10 + 10;
	var v11 : i64 = g11 + 10;
	var v12 : i64 = g12 + 10;
	var v13 : i64 = g13 + 10;
	var t : i64 = v0 + v1;
	return v0 + v0 + v1 + v1 + v2 + v2 + v3 + v3 + v4 + v4
		+ v5 + v5 + v6 + v6 + v7 + v7 + v8 + v8 + v9 + v9
		+ v10 + v10 + v11 + v11 + v12 + v12 + v13 + v13 + t;
}

fn main() : i64 {
	if (squeeze() == 513) {
		return 0;
	}
	return 1;
}
