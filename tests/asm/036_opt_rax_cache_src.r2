// OPT: %rax reuse cache (rax_v) + alias folding across MOVE chains hot.
// `t = i; u = t` folds `u` as alias of `t` (no slot, no MOVE); the remaining
// `v = u` reuses `%rax` via the cache/mem forms, and `s + v` reads `v` from
// memory via `op_rax` without rematerializing consts.
// Without the cache+alias each MOVE would be load+store (extra loads/iter).
// grep loop body: one `movq %rax, -X(%rsp)` pair with a single reload between
// (alias chain `t->u->v` collapses one level; see comp_pending A1/A2 limits).
fn main() : i64 {
	var s : i64 = 0;
	for (var i : i64 = 0; i < 8000000; i = i + 1) {
		var t : i64 = i;
		var u : i64 = t;
		var v : i64 = u;
		s = s + v;
	}
	if (s == 31999996000000) {
		return 0;
	}
	return 1;
}
