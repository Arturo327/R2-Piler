struct P { x : i64; y : i64; }
arr g : i64[4] = {10, 20, 30, 40};

fn pick(x : i64[]) : i64[] { return x; }
fn mk(v : i64) : P { return {.x = v, .y = v + 1} as P; }

fn main() : i64 {
	var s = pick(g);
	var n = s.len;
	var p = $s[0];
	var v = @p;
	var m = g[2];
	var e = mk(5);
	var t = e.x + e.y;
	return s[1] + n + v + m + t;
}